package avioflow

/*
#include <avioflow-c-api.h>
*/
import "C"

import (
	"runtime"
	"sync"
)

// Resampler converts sample rates and channel counts for audio that arrives in
// chunks.
//
// Filter state is preserved across Process calls, so consecutive chunks join
// without discontinuities at the boundaries. For a buffer already held in full,
// Resample is simpler.
//
// Flush is not optional: the resampler holds back the last few milliseconds of
// audio internally, and skipping the flush discards them.
//
// A Resampler must be closed when no longer needed.
type Resampler struct {
	mu     sync.Mutex
	handle *C.AvfResampler
}

// NewResampler creates a resampler. Both sample rates in options are required and
// must be greater than zero.
func NewResampler(options *ResampleOptions) (*Resampler, error) {
	if options == nil {
		return nil, &Error{
			Kind:    ErrInvalidArgument,
			Message: "options are required: both sample rates must be set",
		}
	}

	raw := options.toC()
	handle := C.avf_resampler_new(&raw)
	if handle == nil {
		return nil, errorFromNullHandle()
	}

	r := &Resampler{handle: handle}
	// Backstop only: Close is the supported way to release the handle.
	runtime.SetFinalizer(r, (*Resampler).Close)
	return r, nil
}

// Close releases the native handle. It is idempotent.
func (r *Resampler) Close() error {
	r.mu.Lock()
	defer r.mu.Unlock()

	if r.handle == nil {
		return nil
	}
	C.avf_resampler_free(r.handle)
	r.handle = nil
	runtime.SetFinalizer(r, nil)
	return nil
}

// Process resamples one chunk of samples[channel][sample].
//
// It may return fewer samples than the rate ratio suggests, because the resampler
// buffers samples internally to keep filter continuity; Flush emits the remainder.
// All channels must have the same length, and the channel count must not change
// between calls.
func (r *Resampler) Process(samples [][]float32) ([][]float32, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.handle == nil {
		return nil, ErrClosed
	}

	channels, err := newChannelArray(samples)
	if err != nil {
		return nil, err
	}
	defer channels.release()

	var out *C.AvfSamples
	if err := check(C.avf_resampler_process(r.handle, channels.data,
		channels.count, channels.numSamples, &out)); err != nil {
		return nil, err
	}
	return samplesFromC(out), nil
}

// Flush drains the samples still buffered inside the resampler.
//
// Call it once after the final Process call.
func (r *Resampler) Flush() ([][]float32, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.handle == nil {
		return nil, ErrClosed
	}

	var out *C.AvfSamples
	if err := check(C.avf_resampler_flush(r.handle, &out)); err != nil {
		return nil, err
	}
	return samplesFromC(out), nil
}

// OutputSampleRate returns the rate the resampler was configured with.
func (r *Resampler) OutputSampleRate() (int32, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.handle == nil {
		return 0, ErrClosed
	}

	var rate C.int32_t
	if err := check(C.avf_resampler_output_sample_rate(r.handle, &rate)); err != nil {
		return 0, err
	}
	return int32(rate), nil
}

// OutputNumChannels returns the output channel count, which is zero until the
// first Process call reveals the input channel count.
func (r *Resampler) OutputNumChannels() (int32, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.handle == nil {
		return 0, ErrClosed
	}

	var channels C.int32_t
	if err := check(C.avf_resampler_output_num_channels(r.handle, &channels)); err != nil {
		return 0, err
	}
	return int32(channels), nil
}

// Resample converts a complete buffer in one call, flushing internally so no
// samples are lost.
//
// Pass nil for outputNumChannels to keep the input channel count. For audio
// arriving in chunks use a Resampler instead: calling this per chunk would reset
// the filter state and introduce a discontinuity at every boundary.
func Resample(samples [][]float32, inputSampleRate, outputSampleRate int32,
	outputNumChannels *int32) ([][]float32, error) {

	input, err := newChannelArray(samples)
	if err != nil {
		return nil, err
	}
	defer input.release()

	var channels C.int32_t
	var hasChannels C.int32_t
	if outputNumChannels != nil {
		channels = C.int32_t(*outputNumChannels)
		hasChannels = 1
	}

	var out *C.AvfSamples
	if err := check(C.avf_resample(input.data, input.count, input.numSamples,
		C.int32_t(inputSampleRate), C.int32_t(outputSampleRate),
		channels, hasChannels, &out)); err != nil {
		return nil, err
	}
	return samplesFromC(out), nil
}
