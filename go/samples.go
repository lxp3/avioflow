package avioflow

/*
#include <stdlib.h>
#include <avioflow-c-api.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// samplesFromC copies an owned AvfSamples into Go slices and frees it.
//
// The C buffer is always released, including on the empty paths, so callers do
// not have to.
func samplesFromC(samples *C.AvfSamples) [][]float32 {
	if samples == nil {
		return nil
	}
	defer C.avf_samples_free(samples)

	numChannels := int(C.avf_samples_num_channels(samples))
	numSamples := int(C.avf_samples_num_samples(samples))
	if numChannels <= 0 || numSamples <= 0 {
		return nil
	}

	out := make([][]float32, numChannels)
	for c := 0; c < numChannels; c++ {
		data := C.avf_samples_channel(samples, C.int32_t(c))
		if data == nil {
			out[c] = nil
			continue
		}
		// Copy out of C memory: the backing buffer is freed above, and Go slices
		// must never alias memory the runtime does not own.
		channel := make([]float32, numSamples)
		copy(channel, unsafe.Slice((*float32)(unsafe.Pointer(data)), numSamples))
		out[c] = channel
	}
	return out
}

// channelArray holds planar input in C memory, in the layout the C ABI expects.
//
// Both the pointer array and the sample data are copied into C memory. Neither
// shortcut works: cgo rejects passing a Go pointer that points to other Go
// pointers, which is what a []*C.float over [][]float32 is, and storing Go
// pointers into a C-allocated array is also rejected because it hides them from
// the garbage collector. Copying costs one pass over the samples per call and is
// what makes the call unambiguously safe.
type channelArray struct {
	// data is the C-allocated array of channel pointers, or nil when empty.
	data **C.float
	// blocks are the C-allocated per-channel sample buffers, freed by release.
	blocks []unsafe.Pointer
	// count is the number of channels.
	count C.int32_t
	// numSamples is the length of every channel.
	numSamples C.int64_t
}

// release frees the C memory. Call it with defer immediately after a successful
// newChannelArray.
func (c *channelArray) release() {
	for _, block := range c.blocks {
		C.free(block)
	}
	c.blocks = nil
	if c.data != nil {
		C.free(unsafe.Pointer(c.data))
		c.data = nil
	}
}

// newChannelArray validates planar input and copies it into C memory.
func newChannelArray(samples [][]float32) (*channelArray, error) {
	result := &channelArray{}
	if len(samples) == 0 {
		return result, nil
	}

	length := len(samples[0])
	for i, channel := range samples {
		if len(channel) != length {
			return nil, &Error{
				Kind: ErrInvalidArgument,
				Message: fmt.Sprintf(
					"all channels must have the same length: channel 0 has %d samples, channel %d has %d",
					length, i, len(channel)),
			}
		}
	}

	result.count = C.int32_t(len(samples))
	result.numSamples = C.int64_t(length)

	array := C.malloc(C.size_t(len(samples)) * C.size_t(unsafe.Sizeof(uintptr(0))))
	if array == nil {
		return nil, outOfMemory()
	}
	result.data = (**C.float)(array)
	view := unsafe.Slice(result.data, len(samples))

	for i, channel := range samples {
		if length == 0 {
			// Nothing to copy; the C side reads no samples when the count is 0.
			view[i] = nil
			continue
		}
		block := C.malloc(C.size_t(length) * C.size_t(unsafe.Sizeof(C.float(0))))
		if block == nil {
			result.release()
			return nil, outOfMemory()
		}
		result.blocks = append(result.blocks, block)
		copy(unsafe.Slice((*float32)(block), length), channel)
		view[i] = (*C.float)(block)
	}
	return result, nil
}

func outOfMemory() error {
	return &Error{Kind: ErrRuntime, Message: "out of memory building the channel array"}
}
