package avioflow

/*
#include <avioflow-c-api.h>
*/
import "C"

import (
	"errors"
	"fmt"
)

// Sentinel errors for use with errors.Is.
var (
	// ErrInvalidArgument means an argument was rejected before any work happened.
	ErrInvalidArgument = errors.New("avioflow: invalid argument")
	// ErrRuntime means the operation failed while running, for example because a
	// file could not be opened or a codec is unsupported.
	ErrRuntime = errors.New("avioflow: runtime error")
	// ErrUnknown means the native layer failed without classifying the cause.
	ErrUnknown = errors.New("avioflow: unknown error")
	// ErrClosed means the handle has already been closed.
	ErrClosed = errors.New("avioflow: handle is closed")
)

// Error is the concrete error type returned by failing avioflow operations. It
// carries the message from the native layer and unwraps to one of the sentinels
// above, so both errors.Is and message inspection work:
//
//	if errors.Is(err, avioflow.ErrInvalidArgument) { ... }
type Error struct {
	// Kind classifies the failure. Compare with the sentinel errors via errors.Is
	// rather than switching on this directly.
	Kind error
	// Message is the text recorded by the native layer.
	Message string
}

func (e *Error) Error() string {
	if e.Message == "" {
		return e.Kind.Error()
	}
	return fmt.Sprintf("avioflow: %s", e.Message)
}

// Unwrap returns the sentinel for this failure so errors.Is matches it.
func (e *Error) Unwrap() error { return e.Kind }

// check converts a C status code into an error, reading the thread-local message
// the native layer recorded for the call that just failed.
//
// It must be called immediately after the C call, before anything else on this
// goroutine's thread can overwrite that message.
func check(status C.int) error {
	if status == C.AVF_OK {
		return nil
	}
	return errorFromStatus(status)
}

// errorFromNullHandle builds the error for a constructor that signalled failure
// by returning NULL. Such functions have no status code of their own, so the
// classification is read back from the thread-local the native layer set.
func errorFromNullHandle() error {
	status := C.avf_last_error_code()
	if status == C.AVF_OK {
		// A NULL handle always means failure, so never report success even if the
		// code was somehow not set.
		status = C.AVF_ERR_RUNTIME
	}
	return errorFromStatus(status)
}

func errorFromStatus(status C.int) error {
	var kind error
	switch status {
	case C.AVF_ERR_INVALID_ARGUMENT:
		kind = ErrInvalidArgument
	case C.AVF_ERR_RUNTIME:
		kind = ErrRuntime
	default:
		kind = ErrUnknown
	}

	// avf_last_error always returns a valid NUL-terminated pointer, never NULL.
	message := C.GoString(C.avf_last_error())
	if message == "" {
		message = fmt.Sprintf("call failed with status %d", int(status))
	}
	return &Error{Kind: kind, Message: message}
}
