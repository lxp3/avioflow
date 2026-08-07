/*
Package avioflow provides Go bindings for AvioFlow, a high-performance audio
library built on FFmpeg.

The package wraps the same C++ core as the Python, Node.js, Java, Rust and
WebAssembly bindings, reached through the C ABI the core exports
(avioflow/include/avioflow-c-api.h).

# Prerequisite

Unlike the Rust crate, cgo cannot compile the native core during "go build", so
the avioflow C library must be installed first:

	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
	cmake --build build
	cmake --install build --prefix /usr/local

The package finds it through pkg-config. If the library is installed somewhere
pkg-config does not search, point it there:

	export PKG_CONFIG_PATH=/your/prefix/lib/pkgconfig

# Sample layout

Samples are always planar float32: samples[channel][sample], with every channel
the same length. A ragged input is rejected rather than truncated.

# Resource management

Decoder, Encoder and Resampler each hold a native handle and must be closed:

	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		return err
	}
	defer dec.Close()

Close is idempotent and safe to call from multiple goroutines. A finalizer
releases a handle that was never closed, but relying on it is not supported: the
timing is undefined and native memory can accumulate.

# Concurrency

Each handle serializes its own calls with a mutex, so concurrent use will not
corrupt state. Interleaving calls on one decoder from several goroutines is still
a logic error, because decoding is inherently sequential.

# Basic use

	dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{
		OutputSampleRate: avioflow.Int(16000),
	})
	if err != nil {
		return err
	}
	defer dec.Close()

	meta, err := dec.LoadFile("audio.mp3")
	if err != nil {
		return err
	}

	samples, err := dec.GetSamples()
	if err != nil {
		return err
	}
	fmt.Printf("%.1fs, %d Hz, %d channels\n", meta.Duration, meta.SampleRate, len(samples))
*/
package avioflow
