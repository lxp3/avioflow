package avioflow_test

import (
	"math"
	"path/filepath"
	"runtime"
	"testing"
)

// Known properties of public/wavs/TownTheme.mp3, matching the constants in
// avioflow/bin/decoder-resample-test.cpp.
const (
	sourceSampleRate = 44100
	sourceChannels   = 2
	sourceSamples    = 4297722
)

// Only rounding of the rate ratio may move a resampled count; the resampler tail
// is drained, so a larger gap means samples are being dropped. Mirrors
// SAMPLE_COUNT_TOLERANCE in the C++ tests.
const sampleCountTolerance = 2

// testAudioPath resolves the fixture from this source file's directory rather
// than the process working directory, so tests work however go test is invoked.
func testAudioPath(t *testing.T) string {
	t.Helper()
	_, thisFile, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("cannot determine the test source directory")
	}
	return filepath.Join(filepath.Dir(thisFile), "..", "public", "wavs", "TownTheme.mp3")
}

func assertSampleCount(t *testing.T, actual int, expected int64, context string) {
	t.Helper()
	diff := int64(actual) - expected
	if diff < -sampleCountTolerance || diff > sampleCountTolerance {
		t.Fatalf("%s: expected ~%d samples, got %d (diff %d)", context, expected, actual, diff)
	}
}

// makeSine generates a sine wave, so output can be checked for amplitude rather
// than only for length.
func makeSine(numChannels, numSamples, sampleRate int, freq float64) [][]float32 {
	out := make([][]float32, numChannels)
	for c := range out {
		out[c] = make([]float32, numSamples)
		for i := 0; i < numSamples; i++ {
			t := float64(i) / float64(sampleRate)
			out[c][i] = float32(math.Sin(2 * math.Pi * freq * t))
		}
	}
	return out
}

func peak(channel []float32) float32 {
	var max float32
	for _, v := range channel {
		if v < 0 {
			v = -v
		}
		if v > max {
			max = v
		}
	}
	return max
}
