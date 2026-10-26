package avioflow

/*
#include <avioflow-c-api.h>
*/
import "C"

// StreamInfo describes one stream of an opened container: audio, video,
// subtitles and so on.
type StreamInfo struct {
	// Index is the value to pass as StreamOptions.AudioStreamIndex.
	Index int32
	// MediaType is "audio", "video", "subtitle", "data" or "attachment".
	MediaType string
	// Codec is the codec name, for example "aac" or "h264".
	Codec string
	// Language is the ISO 639 tag from the container. May be empty.
	Language string
	// SampleRate in Hz. Audio streams only, otherwise zero.
	SampleRate int32
	// NumChannels for audio streams, otherwise zero.
	NumChannels int32
	// BitRate in bits per second. Zero if unknown.
	BitRate int64
	// Duration in seconds. Zero if unknown.
	Duration float64
	// IsDefault reports whether the container flags this as the default stream.
	IsDefault bool
}

// streamsFromC copies an owned AvfStreamList into Go values and frees it.
func streamsFromC(list *C.AvfStreamList) ([]StreamInfo, error) {
	if list == nil {
		return nil, nil
	}
	defer C.avf_stream_list_free(list)

	n := int(C.avf_stream_list_size(list))
	out := make([]StreamInfo, 0, n)
	for i := 0; i < n; i++ {
		var raw C.AvfStreamInfo
		if err := check(C.avf_stream_list_get(list, C.size_t(i), &raw)); err != nil {
			return nil, err
		}
		out = append(out, StreamInfo{
			Index:       int32(raw.index),
			MediaType:   C.GoString(&raw.media_type[0]),
			Codec:       C.GoString(&raw.codec[0]),
			Language:    C.GoString(&raw.language[0]),
			SampleRate:  int32(raw.sample_rate),
			NumChannels: int32(raw.num_channels),
			BitRate:     int64(raw.bit_rate),
			Duration:    float64(raw.duration),
			IsDefault:   raw.is_default != 0,
		})
	}
	return out, nil
}
