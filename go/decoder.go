package avioflow

/*
#include <stdlib.h>
#include <avioflow-c-api.h>
*/
import "C"

import (
	"runtime"
	"sync"
	"unsafe"
)

// Decoder decodes audio from a file, URL, memory buffer, device, or a pushed byte
// stream.
//
// A Decoder must be closed when no longer needed. It is safe to use from multiple
// goroutines in the sense that calls are serialized and will not corrupt state,
// but decoding is sequential, so interleaving calls on one Decoder is a logic
// error.
type Decoder struct {
	mu     sync.Mutex
	handle *C.AvfDecoder
}

// NewDecoder creates a decoder. Pass nil for default options, which preserve the
// source format.
func NewDecoder(options *StreamOptions) (*Decoder, error) {
	raw, free := options.toC()
	defer free()

	handle := C.avf_decoder_new(&raw)
	if handle == nil {
		return nil, errorFromNullHandle()
	}

	d := &Decoder{handle: handle}
	// Backstop only: Close is the supported way to release the handle.
	runtime.SetFinalizer(d, (*Decoder).Close)
	return d, nil
}

// Close releases the native handle. It is idempotent, so calling it twice, or
// after a failure, is safe.
func (d *Decoder) Close() error {
	d.mu.Lock()
	defer d.mu.Unlock()

	if d.handle == nil {
		return nil
	}
	C.avf_decoder_free(d.handle)
	d.handle = nil
	runtime.SetFinalizer(d, nil)
	return nil
}

// LoadFile opens a file path, URL, or device identifier and returns the stream
// metadata.
//
// When OutputSampleRate is set, the returned metadata still describes the source:
// the resampler is not configured until the first frame is decoded, so the new
// rate appears in Metadata only after decoding.
func (d *Decoder) LoadFile(source string) (Metadata, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return Metadata{}, ErrClosed
	}

	path := C.CString(source)
	defer C.free(unsafe.Pointer(path))

	var raw C.AvfMetadata
	if err := check(C.avf_decoder_load_file(d.handle, path, &raw)); err != nil {
		return Metadata{}, err
	}
	return metadataFromC(&raw), nil
}

// LoadBuffer opens complete audio file bytes held in memory.
func (d *Decoder) LoadBuffer(data []byte) (Metadata, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return Metadata{}, ErrClosed
	}

	var raw C.AvfMetadata
	var ptr *C.uint8_t
	if len(data) > 0 {
		ptr = (*C.uint8_t)(unsafe.Pointer(&data[0]))
	}
	err := check(C.avf_decoder_load_buffer(d.handle, ptr, C.size_t(len(data)), &raw))
	runtime.KeepAlive(data)
	if err != nil {
		return Metadata{}, err
	}
	return metadataFromC(&raw), nil
}

// Feed pushes encoded bytes for streaming decode.
//
// Requires StreamOptions.InputFormat to have been set. The first Feed call starts
// stream mode.
func (d *Decoder) Feed(data []byte) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return ErrClosed
	}

	var ptr *C.uint8_t
	if len(data) > 0 {
		ptr = (*C.uint8_t)(unsafe.Pointer(&data[0]))
	}
	err := check(C.avf_decoder_feed(d.handle, ptr, C.size_t(len(data))))
	runtime.KeepAlive(data)
	return err
}

// Flush marks streaming input complete so buffered and codec-delayed frames can
// still be drained. It discards nothing.
func (d *Decoder) Flush() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return ErrClosed
	}
	return check(C.avf_decoder_flush(d.handle))
}

// GetSamples decodes all remaining samples as samples[channel][sample].
func (d *Decoder) GetSamples() ([][]float32, error) {
	return d.getSamples(0, 0, false)
}

// GetSamplesRange decodes the half-open range [startSeconds, stopSeconds).
//
// Range decoding requires offline mode, that is a loaded file or buffer. Each call
// seeks independently, so one decoder can serve many ranges. In stream mode use
// GetSamples, which drains whatever is currently buffered.
func (d *Decoder) GetSamplesRange(startSeconds, stopSeconds float64) ([][]float32, error) {
	return d.getSamples(startSeconds, stopSeconds, true)
}

// GetSamplesFrom decodes from startSeconds to the end of the stream.
func (d *Decoder) GetSamplesFrom(startSeconds float64) ([][]float32, error) {
	return d.getSamples(startSeconds, 0, false)
}

func (d *Decoder) getSamples(start, stop float64, hasStop bool) ([][]float32, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return nil, ErrClosed
	}

	var hasStopFlag C.int32_t
	if hasStop {
		hasStopFlag = 1
	}

	var samples *C.AvfSamples
	if err := check(C.avf_decoder_get_samples(d.handle, C.double(start), C.double(stop),
		hasStopFlag, &samples)); err != nil {
		return nil, err
	}
	return samplesFromC(samples), nil
}

// GetFrame decodes the next frame, returning nil at end of stream or when
// streaming input needs more data.
//
// The samples are copied out of the decoder's buffers. The C ABI exposes those
// buffers directly for zero-copy access, but Go cannot express a lifetime tied to
// the next decode call, so a borrowed slice would become a use-after-free. Use the
// C or Rust API if zero-copy decoding matters.
func (d *Decoder) GetFrame() ([][]float32, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return nil, ErrClosed
	}

	var frame C.AvfFrame
	if err := check(C.avf_decoder_get_frame(d.handle, &frame)); err != nil {
		return nil, err
	}
	if frame.data == nil || frame.num_channels <= 0 || frame.num_samples <= 0 {
		return nil, nil
	}

	numChannels := int(frame.num_channels)
	numSamples := int(frame.num_samples)
	channels := unsafe.Slice(frame.data, numChannels)

	out := make([][]float32, numChannels)
	for c := 0; c < numChannels; c++ {
		out[c] = make([]float32, numSamples)
		copy(out[c], unsafe.Slice((*float32)(unsafe.Pointer(channels[c])), numSamples))
	}
	return out, nil
}

// IsFinished reports whether the stream is exhausted.
func (d *Decoder) IsFinished() (bool, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return false, ErrClosed
	}

	var finished C.int32_t
	if err := check(C.avf_decoder_is_finished(d.handle, &finished)); err != nil {
		return false, err
	}
	return finished != 0, nil
}

// Metadata returns the current stream metadata.
func (d *Decoder) Metadata() (Metadata, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.handle == nil {
		return Metadata{}, ErrClosed
	}

	var raw C.AvfMetadata
	if err := check(C.avf_decoder_get_metadata(d.handle, &raw)); err != nil {
		return Metadata{}, err
	}
	return metadataFromC(&raw), nil
}
