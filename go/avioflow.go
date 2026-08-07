package avioflow

/*
#cgo pkg-config: avioflow
#include <stdlib.h>
#include <avioflow-c-api.h>
*/
import "C"

import (
	"unsafe"
)

// LogLevel names an FFmpeg logging verbosity.
type LogLevel string

// Log levels accepted by SetLogLevel, from least to most verbose.
const (
	LogQuiet   LogLevel = "quiet"
	LogPanic   LogLevel = "panic"
	LogFatal   LogLevel = "fatal"
	LogError   LogLevel = "error"
	LogWarning LogLevel = "warning"
	LogInfo    LogLevel = "info"
	LogVerbose LogLevel = "verbose"
	LogDebug   LogLevel = "debug"
	LogTrace   LogLevel = "trace"
)

// SetLogLevel sets the FFmpeg log level.
//
// Pass an empty string to restore the default, which is "error" unless
// AVIOFLOW_LOG_LEVEL is set in the environment. An unrecognized level is ignored
// by the native layer rather than reported as an error.
func SetLogLevel(level LogLevel) {
	if level == "" {
		C.avf_set_log_level(nil)
		return
	}
	text := C.CString(string(level))
	defer C.free(unsafe.Pointer(text))
	C.avf_set_log_level(text)
}

// DeviceInfo describes an audio input or output device.
type DeviceInfo struct {
	// Name is the identifier to pass to Decoder.LoadFile to open this device.
	Name string
	// Description is a human-readable device name.
	Description string
	// IsOutput reports whether this is an output or loopback device.
	IsOutput bool
}

// SupportedDecoders returns the audio decoders this build supports, such as "mp3".
func SupportedDecoders() ([]string, error) {
	return stringList(func(out **C.AvfStringList) C.int {
		return C.avf_get_supported_decoders(out)
	})
}

// SupportedEncoders returns the audio encoders this build supports, such as
// "pcm_s16le".
func SupportedEncoders() ([]string, error) {
	return stringList(func(out **C.AvfStringList) C.int {
		return C.avf_get_supported_encoders(out)
	})
}

// SupportedInputFormats returns the input formats (demuxers) this build supports.
func SupportedInputFormats() ([]string, error) {
	return stringList(func(out **C.AvfStringList) C.int {
		return C.avf_get_supported_input_formats(out)
	})
}

// SupportedOutputFormats returns the output formats (muxers) this build supports.
func SupportedOutputFormats() ([]string, error) {
	return stringList(func(out **C.AvfStringList) C.int {
		return C.avf_get_supported_output_formats(out)
	})
}

// ListAudioDevices enumerates the available audio devices.
func ListAudioDevices() ([]DeviceInfo, error) {
	var list *C.AvfDeviceList
	if err := check(C.avf_list_audio_devices(&list)); err != nil {
		return nil, err
	}
	if list == nil {
		return nil, nil
	}
	defer C.avf_device_list_free(list)

	count := int(C.avf_device_list_size(list))
	devices := make([]DeviceInfo, 0, count)
	for i := 0; i < count; i++ {
		index := C.size_t(i)
		devices = append(devices, DeviceInfo{
			Name:        C.GoString(C.avf_device_list_name(list, index)),
			Description: C.GoString(C.avf_device_list_description(list, index)),
			IsOutput:    C.avf_device_list_is_output(list, index) != 0,
		})
	}
	return devices, nil
}

// stringList runs a call that yields an owned string list and copies it out.
func stringList(call func(**C.AvfStringList) C.int) ([]string, error) {
	var list *C.AvfStringList
	if err := check(call(&list)); err != nil {
		return nil, err
	}
	if list == nil {
		return nil, nil
	}
	defer C.avf_string_list_free(list)

	count := int(C.avf_string_list_size(list))
	items := make([]string, 0, count)
	for i := 0; i < count; i++ {
		items = append(items, C.GoString(C.avf_string_list_get(list, C.size_t(i))))
	}
	return items, nil
}
