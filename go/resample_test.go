package avioflow_test

import (
	"errors"
	"testing"

	avioflow "github.com/lxp3/avioflow/go"
)

func TestOneShotDownsample(t *testing.T) {
	input := makeSine(2, 44100, 44100, 440)

	out, err := avioflow.Resample(input, 44100, 16000, nil)
	if err != nil {
		t.Fatal(err)
	}

	if len(out) != 2 {
		t.Fatalf("got %d channels, want 2", len(out))
	}
	assertSampleCount(t, len(out[0]), 16000, "44100 -> 16000")
	if len(out[1]) != len(out[0]) {
		t.Errorf("channel lengths differ: %d vs %d", len(out[1]), len(out[0]))
	}

	// 440 Hz sits well below the 8 kHz Nyquist limit, so amplitude survives.
	if p := peak(out[0]); p < 0.9 || p > 1.1 {
		t.Errorf("peak amplitude = %v, want ~1.0", p)
	}
}

func TestUpsample(t *testing.T) {
	out, err := avioflow.Resample(makeSine(2, 16000, 16000, 440), 16000, 44100, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 2 {
		t.Fatalf("got %d channels, want 2", len(out))
	}
	assertSampleCount(t, len(out[0]), 44100, "16000 -> 44100")
}

func TestEqualRatesPassThrough(t *testing.T) {
	out, err := avioflow.Resample(makeSine(1, 4096, 16000, 440), 16000, 16000, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 1 || len(out[0]) != 4096 {
		t.Fatalf("got %dx%d, want 1x4096", len(out), len(out[0]))
	}
}

func TestResampleDownmixToMono(t *testing.T) {
	out, err := avioflow.Resample(makeSine(2, 16000, 16000, 440), 16000, 16000, avioflow.Int(1))
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 1 || len(out[0]) != 16000 {
		t.Fatalf("got %dx%d, want 1x16000", len(out), len(out[0]))
	}
}

func TestFlushRecoversTheTail(t *testing.T) {
	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  44100,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	body, err := r.Process(makeSine(1, 44100, 44100, 440))
	if err != nil {
		t.Fatal(err)
	}
	tail, err := r.Flush()
	if err != nil {
		t.Fatal(err)
	}

	withoutFlush := len(body[0])
	withFlush := withoutFlush + len(tail[0])

	// The contract: only after Flush does the count match the rate ratio. How much
	// the resampler withholds is an internal detail, so assert on the total.
	if withFlush < withoutFlush {
		t.Fatal("flush lost samples")
	}
	assertSampleCount(t, withFlush, 16000, "process + flush")
	if diff := int64(withoutFlush) - 16000; diff > -sampleCountTolerance {
		t.Errorf("expected Process alone to fall short of 16000, got %d", withoutFlush)
	}
}

func TestChunkedMatchesOneShot(t *testing.T) {
	const rate = 48000
	input := makeSine(1, rate, rate, 220)

	expected, err := avioflow.Resample(input, rate, 16000, nil)
	if err != nil {
		t.Fatal(err)
	}

	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  rate,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	var chunked []float32
	for i := 0; i < rate; i += 1000 {
		end := i + 1000
		if end > rate {
			end = rate
		}
		part, err := r.Process([][]float32{input[0][i:end]})
		if err != nil {
			t.Fatal(err)
		}
		if len(part) > 0 {
			chunked = append(chunked, part[0]...)
		}
	}
	tail, err := r.Flush()
	if err != nil {
		t.Fatal(err)
	}
	if len(tail) > 0 {
		chunked = append(chunked, tail[0]...)
	}

	// Filter state carries across chunks, so results match sample for sample.
	if len(chunked) != len(expected[0]) {
		t.Fatalf("chunked length %d, one-shot %d", len(chunked), len(expected[0]))
	}
	var maxDiff float32
	for i := range chunked {
		d := chunked[i] - expected[0][i]
		if d < 0 {
			d = -d
		}
		if d > maxDiff {
			maxDiff = d
		}
	}
	if maxDiff > 1e-6 {
		t.Errorf("max sample difference %v, want < 1e-6", maxDiff)
	}
}

func TestResamplerReportsRateAndChannels(t *testing.T) {
	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  44100,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	rate, err := r.OutputSampleRate()
	if err != nil {
		t.Fatal(err)
	}
	if rate != 16000 {
		t.Errorf("OutputSampleRate = %d, want 16000", rate)
	}

	// Channel count is unknown until the first chunk reveals it.
	channels, err := r.OutputNumChannels()
	if err != nil {
		t.Fatal(err)
	}
	if channels != 0 {
		t.Errorf("OutputNumChannels before Process = %d, want 0", channels)
	}

	if _, err := r.Process(makeSine(2, 1000, 44100, 440)); err != nil {
		t.Fatal(err)
	}
	channels, err = r.OutputNumChannels()
	if err != nil {
		t.Fatal(err)
	}
	if channels != 2 {
		t.Errorf("OutputNumChannels after Process = %d, want 2", channels)
	}
}

func TestEmptyInputAndFlush(t *testing.T) {
	out, err := avioflow.Resample(nil, 44100, 16000, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 0 {
		t.Errorf("resampling nothing produced %d channels", len(out))
	}

	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  44100,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	// Flushing before any Process call must not fail.
	tail, err := r.Flush()
	if err != nil {
		t.Fatal(err)
	}
	if len(tail) != 0 {
		t.Errorf("flush before process produced %d channels", len(tail))
	}
}

func TestResampleRejectsInvalidArguments(t *testing.T) {
	input := makeSine(1, 100, 16000, 440)

	if _, err := avioflow.Resample(input, 0, 16000, nil); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("zero input rate: error = %v, want ErrInvalidArgument", err)
	}
	if _, err := avioflow.Resample(input, 16000, -1, nil); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("negative output rate: error = %v, want ErrInvalidArgument", err)
	}
	if _, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  16000,
		OutputSampleRate: 0,
	}); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("constructor zero rate: error = %v, want ErrInvalidArgument", err)
	}
	if _, err := avioflow.NewResampler(nil); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("nil options: error = %v, want ErrInvalidArgument", err)
	}
}

func TestRaggedChannelsRejected(t *testing.T) {
	ragged := [][]float32{make([]float32, 100), make([]float32, 50)}

	_, err := avioflow.Resample(ragged, 16000, 8000, nil)
	if !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Fatalf("error = %v, want ErrInvalidArgument", err)
	}

	// Caught in Go before reaching the C ABI, so the message names the channel.
	var avfErr *avioflow.Error
	if errors.As(err, &avfErr) && avfErr.Message == "" {
		t.Error("ragged input produced no explanatory message")
	}
}

func TestChannelCountMustNotChange(t *testing.T) {
	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  16000,
		OutputSampleRate: 8000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	if _, err := r.Process(makeSine(2, 100, 16000, 440)); err != nil {
		t.Fatal(err)
	}
	if _, err := r.Process(makeSine(1, 100, 16000, 440)); !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Errorf("changing channel count: error = %v, want ErrInvalidArgument", err)
	}
}

func TestResamplerCloseIsIdempotent(t *testing.T) {
	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  44100,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}

	if err := r.Close(); err != nil {
		t.Fatalf("first Close: %v", err)
	}
	if err := r.Close(); err != nil {
		t.Fatalf("second Close: %v", err)
	}
	if _, err := r.Flush(); !errors.Is(err, avioflow.ErrClosed) {
		t.Errorf("Flush after Close: error = %v, want ErrClosed", err)
	}
}
