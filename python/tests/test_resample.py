"""Tests for standalone resampling: resample() and AudioResampler."""
import avioflow
import numpy as np
import pytest

# Only rounding of the rate ratio may move the count; the resampler tail is
# drained, so a larger gap means samples are being dropped. Mirrors
# SAMPLE_COUNT_TOLERANCE in avioflow/bin/resample-test.cpp.
SAMPLE_COUNT_TOLERANCE = 2


def make_sine(num_channels, num_samples, sample_rate, freq=440.0):
    """Generate a sine wave so output can be checked for amplitude, not just length."""
    t = np.arange(num_samples, dtype=np.float32) / sample_rate
    wave = np.sin(2 * np.pi * freq * t).astype(np.float32)
    return np.stack([wave] * num_channels)


def assert_sample_count(actual, expected, context):
    diff = actual - expected
    assert abs(diff) <= SAMPLE_COUNT_TOLERANCE, \
        f"{context}: expected ~{expected} samples, got {actual} (diff {diff})"


def test_one_shot_downsample():
    samples = make_sine(2, 44100, 44100)
    out = avioflow.resample(samples, 44100, 16000)

    assert out.shape[0] == 2, out.shape
    assert_sample_count(out.shape[1], 16000, "44100 -> 16000")
    assert out.dtype == np.float32, out.dtype

    # 440 Hz sits well below the 8 kHz Nyquist limit, so amplitude survives.
    peak = float(np.abs(out[0]).max())
    assert 0.9 < peak < 1.1, f"peak amplitude was {peak}"
    assert np.isfinite(out).all(), "output contains NaN or Inf"
    print("test_one_shot_downsample passed")


def test_upsample():
    samples = make_sine(2, 16000, 16000)
    out = avioflow.resample(samples, 16000, 44100)

    assert out.shape[0] == 2
    assert_sample_count(out.shape[1], 44100, "16000 -> 44100")
    print("test_upsample passed")


def test_equal_rates_pass_through():
    samples = make_sine(1, 4096, 16000)
    out = avioflow.resample(samples, 16000, 16000)

    assert out.shape == (1, 4096), out.shape
    print("test_equal_rates_pass_through passed")


def test_downmix_to_mono():
    samples = make_sine(2, 16000, 16000)
    out = avioflow.resample(samples, 16000, 16000, output_num_channels=1)

    assert out.shape == (1, 16000), out.shape
    print("test_downmix_to_mono passed")


def test_flush_recovers_tail():
    samples = make_sine(1, 44100, 44100)

    resampler = avioflow.AudioResampler(44100, 16000)
    body = resampler.process(samples)
    tail = resampler.flush()

    without_flush = body.shape[1]
    with_flush = without_flush + tail.shape[1]

    # The contract: only after flush() does the count match the rate ratio. How
    # much the resampler withholds is an internal detail, so assert the total.
    assert with_flush >= without_flush
    assert_sample_count(with_flush, 16000, "process + flush")
    assert abs(without_flush - 16000) > SAMPLE_COUNT_TOLERANCE, \
        f"expected process() alone to fall short, got {without_flush}"
    print(f"test_flush_recovers_tail passed (body={without_flush}, tail={tail.shape[1]})")


def test_chunked_matches_one_shot():
    samples = make_sine(1, 48000, 48000, freq=220.0)
    expected = avioflow.resample(samples, 48000, 16000)

    resampler = avioflow.AudioResampler(48000, 16000)
    parts = [resampler.process(samples[:, i:i + 1000])
             for i in range(0, samples.shape[1], 1000)]
    parts.append(resampler.flush())
    chunked = np.concatenate([p for p in parts if p.size], axis=1)

    # Filter state carries across chunks, so results match sample for sample.
    assert chunked.shape == expected.shape, (chunked.shape, expected.shape)
    max_diff = float(np.abs(chunked - expected).max())
    assert max_diff < 1e-6, f"max sample difference was {max_diff:e}"
    print(f"test_chunked_matches_one_shot passed (max diff {max_diff:e})")


def test_output_rate_and_channels_reported():
    resampler = avioflow.AudioResampler(44100, 16000)

    assert resampler.output_sample_rate == 16000
    # Channel count is unknown until the first chunk reveals it.
    assert resampler.output_num_channels == 0

    resampler.process(make_sine(2, 1000, 44100))
    assert resampler.output_num_channels == 2
    print("test_output_rate_and_channels_reported passed")


def test_accepts_foreign_arrays():
    # The point of the standalone API: resample audio avioflow did not decode.
    float64 = np.sin(np.linspace(0, 1, 44100)).reshape(1, -1)
    out = avioflow.resample(float64, 44100, 16000)
    assert out.shape[0] == 1
    assert_sample_count(out.shape[1], 16000, "float64 input")

    # A non-contiguous view must also work, via the forcecast copy.
    strided = make_sine(2, 88200, 44100)[:, ::2]
    out = avioflow.resample(strided, 22050, 16000)
    assert out.shape[0] == 2
    print("test_accepts_foreign_arrays passed")


def test_empty_flush():
    resampler = avioflow.AudioResampler(44100, 16000)
    tail = resampler.flush()
    assert tail.size == 0, tail.shape
    print("test_empty_flush passed")


@pytest.mark.parametrize(
    "call",
    [
        pytest.param(
            lambda samples: avioflow.resample(samples, 0, 16000),
            id="zero-input-rate",
        ),
        pytest.param(
            lambda samples: avioflow.resample(samples, 16000, -1),
            id="negative-output-rate",
        ),
        pytest.param(
            lambda samples: avioflow.resample(samples[0], 16000, 8000),
            id="one-dimensional-array",
        ),
        pytest.param(
            lambda samples: avioflow.AudioResampler(16000, 0),
            id="constructor-zero-rate",
        ),
    ],
)
def test_invalid_arguments(call):
    samples = make_sine(1, 100, 16000)
    with pytest.raises(ValueError):
        call(samples)


def test_rejects_channel_count_change():
    resampler = avioflow.AudioResampler(16000, 8000)
    resampler.process(make_sine(2, 100, 16000))
    with pytest.raises(ValueError):
        resampler.process(make_sine(1, 100, 16000))
