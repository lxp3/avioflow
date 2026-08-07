package avioflow_test

import (
	"testing"

	avioflow "github.com/lxp3/avioflow/go"
)

// Mirrors the chunked-resampling example in README.md, so the documented code is
// known to compile and produce the full sample count.
func TestReadmeChunkedExample(t *testing.T) {
	input := makeSine(1, 48000, 48000, 220)
	chunks := [][][]float32{}
	for i := 0; i < 48000; i += 1000 {
		chunks = append(chunks, [][]float32{input[0][i : i+1000]})
	}

	r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
		InputSampleRate:  48000,
		OutputSampleRate: 16000,
	})
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	var out [][]float32
	appendPart := func(part [][]float32) {
		if out == nil {
			out = make([][]float32, len(part))
		}
		for c := range part {
			out[c] = append(out[c], part[c]...)
		}
	}

	for _, chunk := range chunks {
		part, err := r.Process(chunk)
		if err != nil {
			t.Fatal(err)
		}
		appendPart(part)
	}
	tail, err := r.Flush()
	if err != nil {
		t.Fatal(err)
	}
	appendPart(tail)

	assertSampleCount(t, len(out[0]), 16000, "README chunked example")
}
