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

// Encoder writes in-memory planar float samples to audio files.
//
// One Encoder can write several files. For a single write, SaveAudio is more
// direct. An Encoder must be closed when no longer needed.
type Encoder struct {
	mu     sync.Mutex
	handle *C.AvfEncoder
}

// NewEncoder creates an encoder. Pass nil for default options, which infer the
// format from the container and the input samples.
func NewEncoder(options *WriteOptions) (*Encoder, error) {
	raw, free := options.toC()
	defer free()

	handle := C.avf_encoder_new(&raw)
	if handle == nil {
		return nil, errorFromNullHandle()
	}

	e := &Encoder{handle: handle}
	// Backstop only: Close is the supported way to release the handle.
	runtime.SetFinalizer(e, (*Encoder).Close)
	return e, nil
}

// Close releases the native handle. It is idempotent.
func (e *Encoder) Close() error {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.handle == nil {
		return nil
	}
	C.avf_encoder_free(e.handle)
	e.handle = nil
	runtime.SetFinalizer(e, nil)
	return nil
}

// Save writes samples, laid out as samples[channel][sample], to path.
//
// All channels must have the same length.
func (e *Encoder) Save(path string, samples [][]float32) error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.handle == nil {
		return ErrClosed
	}

	channels, err := newChannelArray(samples)
	if err != nil {
		return err
	}
	defer channels.release()

	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	return check(C.avf_encoder_save(e.handle, cPath, channels.data,
		channels.count, channels.numSamples))
}

// SaveBuffer encodes samples and returns the complete container bytes.
func (e *Encoder) SaveBuffer(samples [][]float32) ([]byte, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.handle == nil { return nil, ErrClosed }
	channels, err := newChannelArray(samples); if err != nil { return nil, err }
	defer channels.release()
	var data *C.uint8_t; var size C.size_t
	status := C.avf_encoder_save_buffer(e.handle, channels.data, channels.count, channels.numSamples, &data, &size)
	if err := check(status); err != nil { return nil, err }
	result := C.GoBytes(unsafe.Pointer(data), C.int(size))
	C.avf_free_buffer(data)
	return result, nil
}

// SaveAudio writes planar float samples to a file in one call.
//
// Pass nil options to infer the format from the file extension and the samples.
func SaveAudio(path string, samples [][]float32, options *WriteOptions) error {
	channels, err := newChannelArray(samples)
	if err != nil {
		return err
	}
	defer channels.release()

	raw, free := options.toC()
	defer free()

	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	return check(C.avf_save_audio(cPath, channels.data,
		channels.count, channels.numSamples, &raw))
}
