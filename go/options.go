package avioflow

/*
#include <stdlib.h>
#include <avioflow-c-api.h>
*/
import "C"

import "unsafe"

// Int returns a pointer to v, for setting the optional integer fields on the
// options structs.
//
// The C ABI distinguishes "unset" from any particular value, so these fields are
// pointers rather than using a sentinel such as -1:
//
//	&StreamOptions{OutputSampleRate: avioflow.Int(16000)}
func Int(v int32) *int32 { return &v }

// Int64 returns a pointer to v, for the optional 64-bit fields.
func Int64(v int64) *int64 { return &v }

// StreamOptions configures a Decoder. A nil field preserves the source format.
type StreamOptions struct {
	// OutputSampleRate resamples decoded audio to this rate in Hz.
	OutputSampleRate *int32
	// OutputNumChannels remixes decoded audio to this channel count.
	OutputNumChannels *int32
	// InputSampleRate is the rate of raw PCM stream input. Required to Feed raw PCM.
	InputSampleRate *int32
	// InputChannels is the channel count of raw PCM stream input. Required to Feed
	// raw PCM.
	InputChannels *int32
	// InputFormat names the input format, for example "s16le" or "mp3". Required
	// to use Feed.
	InputFormat string
}

// toC builds the C representation. The returned free function releases the C
// string the struct points at and must be called after the C call returns.
func (o *StreamOptions) toC() (C.AvfStreamOptions, func()) {
	var raw C.AvfStreamOptions
	if o == nil {
		return raw, func() {}
	}

	if o.OutputSampleRate != nil {
		raw.output_sample_rate = C.int32_t(*o.OutputSampleRate)
		raw.has_output_sample_rate = 1
	}
	if o.OutputNumChannels != nil {
		raw.output_num_channels = C.int32_t(*o.OutputNumChannels)
		raw.has_output_num_channels = 1
	}
	if o.InputSampleRate != nil {
		raw.input_sample_rate = C.int32_t(*o.InputSampleRate)
		raw.has_input_sample_rate = 1
	}
	if o.InputChannels != nil {
		raw.input_channels = C.int32_t(*o.InputChannels)
		raw.has_input_channels = 1
	}

	free := func() {}
	if o.InputFormat != "" {
		format := C.CString(o.InputFormat)
		raw.input_format = format
		free = func() { C.free(unsafe.Pointer(format)) }
	}
	return raw, free
}

// WriteOptions configures an Encoder or SaveAudio. Unset fields are inferred by
// the encoder from the container and the input samples.
type WriteOptions struct {
	// CodecName names the codec, for example "pcm_s16le", "flac", "aac",
	// "libmp3lame" or "libopus".
	CodecName string
	// ContainerFormat names the container, for example "wav", "flac", "mp4",
	// "ogg" or "adts".
	ContainerFormat string
	// SampleFormat names the sample format, for example "s16", "s32", "flt" or
	// "fltp".
	SampleFormat string
	// SampleRate is the output sample rate in Hz.
	SampleRate *int32
	// NumChannels is the output channel count.
	NumChannels *int32
	// BitRate is the output bit rate in bits per second, for lossy codecs.
	BitRate *int64
	// NoOverwrite prevents replacing an existing file.
	//
	// The C++ default is to overwrite, so this is expressed as an opt-out to keep
	// the zero value of WriteOptions matching that default.
	NoOverwrite bool
}

func (o *WriteOptions) toC() (C.AvfWriteOptions, func()) {
	var raw C.AvfWriteOptions
	// Matches the C++ AudioWriteOptions default.
	raw.overwrite = 1
	if o == nil {
		return raw, func() {}
	}

	var allocated []*C.char
	cstring := func(value string) *C.char {
		if value == "" {
			return nil
		}
		text := C.CString(value)
		allocated = append(allocated, text)
		return text
	}

	raw.codec_name = cstring(o.CodecName)
	raw.container_format = cstring(o.ContainerFormat)
	raw.sample_format = cstring(o.SampleFormat)

	if o.SampleRate != nil {
		raw.sample_rate = C.int32_t(*o.SampleRate)
		raw.has_sample_rate = 1
	}
	if o.NumChannels != nil {
		raw.num_channels = C.int32_t(*o.NumChannels)
		raw.has_num_channels = 1
	}
	if o.BitRate != nil {
		raw.bit_rate = C.int64_t(*o.BitRate)
		raw.has_bit_rate = 1
	}
	if o.NoOverwrite {
		raw.overwrite = 0
	}

	return raw, func() {
		for _, text := range allocated {
			C.free(unsafe.Pointer(text))
		}
	}
}

// ResampleOptions configures a Resampler. Both sample rates are required and must
// be greater than zero.
type ResampleOptions struct {
	// InputSampleRate is the source sample rate in Hz.
	InputSampleRate int32
	// OutputSampleRate is the target sample rate in Hz.
	OutputSampleRate int32
	// OutputNumChannels remixes to this channel count. Nil keeps the input count.
	OutputNumChannels *int32
}

func (o *ResampleOptions) toC() C.AvfResampleOptions {
	var raw C.AvfResampleOptions
	if o == nil {
		return raw
	}
	raw.input_sample_rate = C.int32_t(o.InputSampleRate)
	raw.output_sample_rate = C.int32_t(o.OutputSampleRate)
	if o.OutputNumChannels != nil {
		raw.output_num_channels = C.int32_t(*o.OutputNumChannels)
		raw.has_output_num_channels = 1
	}
	return raw
}
