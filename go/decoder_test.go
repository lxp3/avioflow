package avioflow_test

import (
	"errors"
	"os"
	"testing"

	avioflow "github.com/lxp3/avioflow/go"
)

func TestLoadFileReportsMetadata(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadFile(testAudioPath(t))
	if err != nil {
		t.Fatal(err)
	}

	if meta.SampleRate != sourceSampleRate {
		t.Errorf("SampleRate = %d, want %d", meta.SampleRate, sourceSampleRate)
	}
	if meta.NumChannels != sourceChannels {
		t.Errorf("NumChannels = %d, want %d", meta.NumChannels, sourceChannels)
	}
	// The decoder name, not the container's: FFmpeg decodes mp3 with mp3float.
	if meta.Codec != "mp3float" {
		t.Errorf("Codec = %q, want %q", meta.Codec, "mp3float")
	}
	if meta.Container != "mp3" {
		t.Errorf("Container = %q, want %q", meta.Container, "mp3")
	}
	if meta.Duration < 97 || meta.Duration > 98 {
		t.Errorf("Duration = %v, want ~97.5", meta.Duration)
	}
}

func TestGetSamplesReturnsEverySample(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	samples, err := dec.GetSamples()
	if err != nil {
		t.Fatal(err)
	}

	if len(samples) != sourceChannels {
		t.Fatalf("got %d channels, want %d", len(samples), sourceChannels)
	}
	if len(samples[0]) != sourceSamples {
		t.Errorf("got %d samples, want %d", len(samples[0]), sourceSamples)
	}
	if len(samples[1]) != len(samples[0]) {
		t.Errorf("channel lengths differ: %d vs %d", len(samples[1]), len(samples[0]))
	}

	finished, err := dec.IsFinished()
	if err != nil {
		t.Fatal(err)
	}
	if !finished {
		t.Error("IsFinished() = false after decoding everything")
	}

	// Real audio, not silence or garbage.
	if p := peak(samples[0]); p <= 0.01 || p > 2.0 {
		t.Errorf("peak amplitude = %v, want a plausible signal level", p)
	}
}

func TestDecoderResamplesWhenAsked(t *testing.T) {
	const target = 16000

	dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{
		OutputSampleRate: avioflow.Int(target),
	})
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadFile(testAudioPath(t))
	if err != nil {
		t.Fatal(err)
	}
	// LoadFile reports the source stream: the resampler is not configured until
	// the first frame is decoded.
	if meta.SampleRate != sourceSampleRate {
		t.Errorf("LoadFile SampleRate = %d, want the source rate %d", meta.SampleRate, sourceSampleRate)
	}

	samples, err := dec.GetSamples()
	if err != nil {
		t.Fatal(err)
	}

	expected := int64(sourceSamples) * target / sourceSampleRate
	// Exercises the EOF resampler drain: without it the tail is short by ~16.
	assertSampleCount(t, len(samples[0]), expected, "decoder resample to 16 kHz")

	after, err := dec.Metadata()
	if err != nil {
		t.Fatal(err)
	}
	if after.SampleRate != target {
		t.Errorf("Metadata SampleRate after decoding = %d, want %d", after.SampleRate, target)
	}
}

func TestDecoderDownmixesToMono(t *testing.T) {
	dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{
		OutputNumChannels: avioflow.Int(1),
	})
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}
	samples, err := dec.GetSamples()
	if err != nil {
		t.Fatal(err)
	}

	if len(samples) != 1 {
		t.Fatalf("got %d channels, want 1", len(samples))
	}
	if len(samples[0]) != sourceSamples {
		t.Errorf("got %d samples, want %d", len(samples[0]), sourceSamples)
	}
}

func TestGetSamplesRange(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	samples, err := dec.GetSamplesRange(10.0, 20.0)
	if err != nil {
		t.Fatal(err)
	}

	if len(samples) != sourceChannels {
		t.Fatalf("got %d channels, want %d", len(samples), sourceChannels)
	}
	// Range edges land on the enclosing frame, so allow a frame of slack rather
	// than the tight resampling tolerance.
	expected := int64(10 * sourceSampleRate)
	diff := int64(len(samples[0])) - expected
	if diff < -2304 || diff > 2304 {
		t.Errorf("got %d samples for a 10s window, want ~%d (diff %d)", len(samples[0]), expected, diff)
	}
}

func TestRangesAreIndependent(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	first, err := dec.GetSamplesRange(5.0, 6.0)
	if err != nil {
		t.Fatal(err)
	}
	second, err := dec.GetSamplesRange(5.0, 6.0)
	if err != nil {
		t.Fatal(err)
	}
	elsewhere, err := dec.GetSamplesRange(30.0, 31.0)
	if err != nil {
		t.Fatal(err)
	}

	// Each call seeks independently, so the same range yields the same audio.
	if len(first[0]) != len(second[0]) {
		t.Fatalf("repeated range differs in length: %d vs %d", len(first[0]), len(second[0]))
	}
	for i := range first[0] {
		if first[0][i] != second[0][i] {
			t.Fatalf("repeated range differs at sample %d", i)
		}
	}
	if len(elsewhere[0]) == len(first[0]) && sameSamples(first[0], elsewhere[0]) {
		t.Error("a different range returned identical audio")
	}
}

func sameSamples(a, b []float32) bool {
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func TestGetSamplesFrom(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	tail, err := dec.GetSamplesFrom(90.0)
	if err != nil {
		t.Fatal(err)
	}
	if len(tail) != sourceChannels || len(tail[0]) == 0 {
		t.Fatalf("expected audio from 90s to the end, got %d channels", len(tail))
	}
	// ~7.45s remains after 90s of a ~97.45s file.
	seconds := float64(len(tail[0])) / sourceSampleRate
	if seconds < 7.3 || seconds > 7.6 {
		t.Errorf("got %.3fs from 90s, expected ~7.45s", seconds)
	}
}

// Ranges late in the file guard the coordinate-system bug fixed in 3a40a51,
// where a seek reset the decoder's sample counter while the trim bounds stayed
// absolute, so anything past roughly half the duration came back empty.
func TestRangesWorkAcrossTheWholeFile(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	for _, start := range []float64{0, 20, 50, 70, 90} {
		samples, err := dec.GetSamplesRange(start, start+5)
		if err != nil {
			t.Fatalf("range from %.0fs: %v", start, err)
		}
		if len(samples) == 0 || len(samples[0]) == 0 {
			t.Fatalf("range from %.0fs returned nothing", start)
		}
		seconds := float64(len(samples[0])) / sourceSampleRate
		if seconds < 4.99 || seconds > 5.01 {
			t.Errorf("range from %.0fs: got %.3fs, want 5.0s", start, seconds)
		}
	}
}

func TestOpenEndedRangesReachTheEnd(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	// TownTheme.mp3 is ~97.45s.
	for _, tc := range []struct {
		start    float64
		expected float64
	}{{0, 97.45}, {30, 67.45}, {90, 7.45}} {
		samples, err := dec.GetSamplesFrom(tc.start)
		if err != nil {
			t.Fatalf("from %.0fs: %v", tc.start, err)
		}
		if len(samples) == 0 || len(samples[0]) == 0 {
			t.Fatalf("open-ended range from %.0fs returned nothing", tc.start)
		}
		seconds := float64(len(samples[0])) / sourceSampleRate
		if seconds < tc.expected-0.1 || seconds > tc.expected+0.1 {
			t.Errorf("from %.0fs: got %.3fs, want ~%.2fs", tc.start, seconds, tc.expected)
		}
	}
}

func TestGetFrameWalksTheStream(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	frames, total, channels := 0, 0, 0
	for {
		frame, err := dec.GetFrame()
		if err != nil {
			t.Fatal(err)
		}
		if frame == nil {
			break
		}
		frames++
		channels = len(frame)
		total += len(frame[0])
	}

	if frames == 0 {
		t.Fatal("decoded no frames")
	}
	if channels != sourceChannels {
		t.Errorf("got %d channels, want %d", channels, sourceChannels)
	}
	if total != sourceSamples {
		t.Errorf("frames totalled %d samples, want %d", total, sourceSamples)
	}
}

func TestLoadBufferMatchesLoadFile(t *testing.T) {
	data, err := os.ReadFile(testAudioPath(t))
	if err != nil {
		t.Fatal(err)
	}

	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadBuffer(data)
	if err != nil {
		t.Fatal(err)
	}
	samples, err := dec.GetSamples()
	if err != nil {
		t.Fatal(err)
	}

	if meta.SampleRate != sourceSampleRate {
		t.Errorf("SampleRate = %d, want %d", meta.SampleRate, sourceSampleRate)
	}
	if len(samples[0]) != sourceSamples {
		t.Errorf("got %d samples, want %d", len(samples[0]), sourceSamples)
	}
}

func TestMissingFileReportsAnError(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	_, err = dec.LoadFile("/nonexistent/definitely-not-here.mp3")
	if err == nil {
		t.Fatal("loading a missing file should fail")
	}
	if !errors.Is(err, avioflow.ErrRuntime) {
		t.Errorf("error = %v, want it to match ErrRuntime", err)
	}

	// The point is that avf_last_error is wired through, not the exact text.
	var avfErr *avioflow.Error
	if !errors.As(err, &avfErr) {
		t.Fatalf("error %v is not an *avioflow.Error", err)
	}
	if avfErr.Message == "" {
		t.Error("error carried no message from the native layer")
	}
}

func TestInvalidRangeIsRejected(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.LoadFile(testAudioPath(t)); err != nil {
		t.Fatal(err)
	}

	if _, err := dec.GetSamplesRange(20.0, 10.0); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("stop before start: error = %v, want ErrInvalidArgument", err)
	}
	if _, err := dec.GetSamplesFrom(-1.0); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("negative start: error = %v, want ErrInvalidArgument", err)
	}
}

func TestCloseIsIdempotent(t *testing.T) {
	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}

	if err := dec.Close(); err != nil {
		t.Fatalf("first Close: %v", err)
	}
	if err := dec.Close(); err != nil {
		t.Fatalf("second Close: %v", err)
	}

	// Using a closed decoder reports rather than crashing.
	if _, err := dec.LoadFile(testAudioPath(t)); !errors.Is(err, avioflow.ErrClosed) {
		t.Errorf("LoadFile after Close: error = %v, want ErrClosed", err)
	}
	if _, err := dec.GetSamples(); !errors.Is(err, avioflow.ErrClosed) {
		t.Errorf("GetSamples after Close: error = %v, want ErrClosed", err)
	}
}

func TestFormatQueries(t *testing.T) {
	for _, tc := range []struct {
		name string
		fn   func() ([]string, error)
		want string
	}{
		{"decoders", avioflow.SupportedDecoders, "mp3"},
		{"encoders", avioflow.SupportedEncoders, "pcm_s16le"},
		{"input formats", avioflow.SupportedInputFormats, "mp3"},
		{"output formats", avioflow.SupportedOutputFormats, "wav"},
	} {
		values, err := tc.fn()
		if err != nil {
			t.Fatalf("%s: %v", tc.name, err)
		}
		found := false
		for _, v := range values {
			if v == tc.want {
				found = true
				break
			}
		}
		if !found {
			t.Errorf("%s (%d entries) did not include %q", tc.name, len(values), tc.want)
		}
	}
}

func TestSetLogLevel(t *testing.T) {
	// No return value to check; this asserts the calls are safe, including the
	// empty-string reset and an unrecognized level.
	avioflow.SetLogLevel(avioflow.LogQuiet)
	avioflow.SetLogLevel("bogus-level")
	avioflow.SetLogLevel("")
}

func TestListAudioDevices(t *testing.T) {
	// A CI container usually has no devices, so an empty list is a pass; this
	// checks the call and the string marshalling do not fail.
	devices, err := avioflow.ListAudioDevices()
	if err != nil {
		t.Fatal(err)
	}
	for _, d := range devices {
		if d.Name == "" {
			t.Error("a device reported an empty name")
		}
	}
}
