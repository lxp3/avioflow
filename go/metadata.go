package avioflow

/*
#include <avioflow-c-api.h>
*/
import "C"

// Metadata describes an audio stream.
type Metadata struct {
	// Duration in seconds. Zero for live streams.
	Duration float64
	// NumSamples per channel. Updated at end of stream for streaming input.
	NumSamples int64
	// SampleRate in Hz. Reflects OutputSampleRate once resampling is configured.
	SampleRate int32
	// NumChannels is 1 for mono, 2 for stereo.
	NumChannels int32
	// BitRate in bits per second.
	BitRate int64
	// SampleFormat is the source sample format, for example "fltp".
	SampleFormat string
	// Codec is the FFmpeg decoder name, for example "mp3float" for an MP3 file.
	// Compare Container for the container's name.
	Codec string
	// Container is the container format, for example "mp3".
	Container string
}

func metadataFromC(raw *C.AvfMetadata) Metadata {
	return Metadata{
		Duration:    float64(raw.duration),
		NumSamples:  int64(raw.num_samples),
		SampleRate:  int32(raw.sample_rate),
		NumChannels: int32(raw.num_channels),
		BitRate:     int64(raw.bit_rate),
		// The shim always NUL-terminates these fixed-size buffers.
		SampleFormat: C.GoString(&raw.sample_format[0]),
		Codec:        C.GoString(&raw.codec[0]),
		Container:    C.GoString(&raw.container[0]),
	}
}
