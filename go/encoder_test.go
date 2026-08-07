package avioflow_test

import (
	"errors"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"testing"

	avioflow "github.com/lxp3/avioflow/go"
)

// makeRamp generates audio that differs per channel, so a channel mix-up shows up.
func makeRamp(numChannels, numSamples int) [][]float32 {
	out := make([][]float32, numChannels)
	for c := range out {
		out[c] = make([]float32, numSamples)
		for i := 0; i < numSamples; i++ {
			phase := float64(i)/float64(numSamples) + float64(c)*0.25
			out[c][i] = float32(math.Sin(phase*2*math.Pi) * 0.5)
		}
	}
	return out
}

func TestSaveAudioRoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "roundtrip.wav")
	samples := makeRamp(2, 16000)

	err := avioflow.SaveAudio(path, samples, &avioflow.WriteOptions{
		ContainerFormat: "wav",
		CodecName:       "pcm_s16le",
		SampleRate:      avioflow.Int(16000),
		NumChannels:     avioflow.Int(2),
	})
	if err != nil {
		t.Fatal(err)
	}

	if _, err := os.Stat(path); err != nil {
		t.Fatalf("encoder wrote no file: %v", err)
	}

	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	decoded, err := dec.GetSamples()
	if err != nil {
		t.Fatal(err)
	}

	if meta.SampleRate != 16000 {
		t.Errorf("SampleRate = %d, want 16000", meta.SampleRate)
	}
	if meta.NumChannels != 2 {
		t.Errorf("NumChannels = %d, want 2", meta.NumChannels)
	}
	if len(decoded) != 2 || len(decoded[0]) != 16000 {
		t.Fatalf("decoded %dx%d, want 2x16000", len(decoded), len(decoded[0]))
	}

	// pcm_s16le quantizes to 1/32768, so compare with that as the bound.
	var maxDiff float32
	for i := range decoded[0] {
		d := decoded[0][i] - samples[0][i]
		if d < 0 {
			d = -d
		}
		if d > maxDiff {
			maxDiff = d
		}
	}
	if maxDiff > 1e-3 {
		t.Errorf("max round-trip difference %v, want < 1e-3", maxDiff)
	}
}

func TestEncoderWritesMonoFlac(t *testing.T) {
	path := filepath.Join(t.TempDir(), "mono.flac")

	enc, err := avioflow.NewEncoder(&avioflow.WriteOptions{
		ContainerFormat: "flac",
		CodecName:       "flac",
		SampleRate:      avioflow.Int(8000),
		NumChannels:     avioflow.Int(1),
	})
	if err != nil {
		t.Fatal(err)
	}
	defer enc.Close()

	if err := enc.Save(path, makeRamp(1, 8000)); err != nil {
		t.Fatal(err)
	}

	dec, err := avioflow.NewDecoder(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if meta.NumChannels != 1 {
		t.Errorf("NumChannels = %d, want 1", meta.NumChannels)
	}
	if meta.SampleRate != 8000 {
		t.Errorf("SampleRate = %d, want 8000", meta.SampleRate)
	}
	if meta.Codec != "flac" {
		t.Errorf("Codec = %q, want %q", meta.Codec, "flac")
	}
}

func TestOneEncoderWritesSeveralFiles(t *testing.T) {
	dir := t.TempDir()
	samples := makeRamp(1, 4000)

	enc, err := avioflow.NewEncoder(&avioflow.WriteOptions{
		ContainerFormat: "wav",
		CodecName:       "pcm_s16le",
		SampleRate:      avioflow.Int(8000),
		NumChannels:     avioflow.Int(1),
	})
	if err != nil {
		t.Fatal(err)
	}
	defer enc.Close()

	for i := 0; i < 3; i++ {
		path := filepath.Join(dir, fmt.Sprintf("part%d.wav", i))
		if err := enc.Save(path, samples); err != nil {
			t.Fatalf("part %d: %v", i, err)
		}
		if _, err := os.Stat(path); err != nil {
			t.Errorf("part %d was not written: %v", i, err)
		}
	}
}

func TestEncoderRejectsRaggedChannels(t *testing.T) {
	path := filepath.Join(t.TempDir(), "ragged.wav")
	ragged := [][]float32{make([]float32, 100), make([]float32, 50)}

	err := avioflow.SaveAudio(path, ragged, &avioflow.WriteOptions{
		ContainerFormat: "wav",
		SampleRate:      avioflow.Int(8000),
	})
	if !errors.Is(err, avioflow.ErrInvalidArgument) {
		t.Fatalf("error = %v, want ErrInvalidArgument", err)
	}
	if _, statErr := os.Stat(path); statErr == nil {
		t.Error("a file was created despite the failure")
	}
}

func TestUnwritablePathReportsAnError(t *testing.T) {
	err := avioflow.SaveAudio("/nonexistent-directory/out.wav", makeRamp(1, 100),
		&avioflow.WriteOptions{ContainerFormat: "wav", SampleRate: avioflow.Int(8000)})
	if err == nil {
		t.Fatal("writing into a missing directory should fail")
	}

	var avfErr *avioflow.Error
	if !errors.As(err, &avfErr) {
		t.Fatalf("error %v is not an *avioflow.Error", err)
	}
	if avfErr.Message == "" {
		t.Error("error carried no message from the native layer")
	}
}

func TestEncoderCloseIsIdempotent(t *testing.T) {
	enc, err := avioflow.NewEncoder(&avioflow.WriteOptions{
		ContainerFormat: "wav",
		SampleRate:      avioflow.Int(8000),
	})
	if err != nil {
		t.Fatal(err)
	}

	if err := enc.Close(); err != nil {
		t.Fatalf("first Close: %v", err)
	}
	if err := enc.Close(); err != nil {
		t.Fatalf("second Close: %v", err)
	}

	path := filepath.Join(t.TempDir(), "closed.wav")
	if err := enc.Save(path, makeRamp(1, 100)); !errors.Is(err, avioflow.ErrClosed) {
		t.Errorf("Save after Close: error = %v, want ErrClosed", err)
	}
}

// NoOverwrite is expressed as an opt-out so the zero value of WriteOptions
// matches the C++ default of overwriting.
func TestNoOverwriteRefusesExistingFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "existing.wav")
	samples := makeRamp(1, 1000)
	options := &avioflow.WriteOptions{
		ContainerFormat: "wav",
		CodecName:       "pcm_s16le",
		SampleRate:      avioflow.Int(8000),
		NumChannels:     avioflow.Int(1),
	}

	if err := avioflow.SaveAudio(path, samples, options); err != nil {
		t.Fatal(err)
	}

	options.NoOverwrite = true
	if err := avioflow.SaveAudio(path, samples, options); err == nil {
		t.Error("NoOverwrite did not prevent replacing an existing file")
	}
}
