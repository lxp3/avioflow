//! Standalone resampling: one-shot conversion, chunked continuity, the flush
//! contract, downmixing, and argument validation.

mod common;

use avioflow::{resample, AudioResampler, ErrorKind, ResampleOptions};
use common::{assert_sample_count, SAMPLE_COUNT_TOLERANCE};

/// Generates a sine wave, so output can be checked for amplitude rather than
/// only for length.
fn make_sine(
    num_channels: usize,
    num_samples: usize,
    sample_rate: i32,
    freq: f64,
) -> Vec<Vec<f32>> {
    (0..num_channels)
        .map(|_| {
            (0..num_samples)
                .map(|i| {
                    let t = i as f64 / sample_rate as f64;
                    (2.0 * std::f64::consts::PI * freq * t).sin() as f32
                })
                .collect()
        })
        .collect()
}

fn peak(channel: &[f32]) -> f32 {
    channel.iter().fold(0.0f32, |acc, s| acc.max(s.abs()))
}

#[test]
fn one_shot_downsample_keeps_length_and_amplitude() {
    let input = make_sine(2, 44100, 44100, 440.0);

    let output = resample(&input, 44100, 16000, None).unwrap();

    assert_eq!(output.len(), 2);
    assert_sample_count(output[0].len(), 16000, "44100 -> 16000");
    assert_eq!(output[1].len(), output[0].len());

    // 440 Hz sits well below the 8 kHz Nyquist limit, so amplitude survives.
    let amplitude = peak(&output[0]);
    assert!(
        (0.9..1.1).contains(&amplitude),
        "peak amplitude was {amplitude}"
    );
    assert!(output[0].iter().all(|s| s.is_finite()));
}

#[test]
fn upsampling_lengthens_output() {
    let input = make_sine(2, 16000, 16000, 440.0);

    let output = resample(&input, 16000, 44100, None).unwrap();

    assert_eq!(output.len(), 2);
    assert_sample_count(output[0].len(), 44100, "16000 -> 44100");
}

#[test]
fn equal_rates_pass_samples_through() {
    let input = make_sine(1, 4096, 16000, 440.0);

    let output = resample(&input, 16000, 16000, None).unwrap();

    assert_eq!(output.len(), 1);
    assert_eq!(output[0].len(), 4096);
}

#[test]
fn downmix_to_mono() {
    let input = make_sine(2, 16000, 16000, 440.0);

    let output = resample(&input, 16000, 16000, Some(1)).unwrap();

    assert_eq!(output.len(), 1);
    assert_eq!(output[0].len(), 16000);
}

#[test]
fn flush_recovers_the_tail() {
    let input = make_sine(1, 44100, 44100, 440.0);

    let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000)).unwrap();
    let body = resampler.process(&input).unwrap();
    let tail = resampler.flush().unwrap();

    let without_flush = body[0].len();
    let with_flush = without_flush + tail[0].len();

    // The contract: only after flush() does the count match the rate ratio.
    // How much the resampler withholds is an internal detail, so assert on the
    // total rather than on a specific tail length.
    assert!(with_flush >= without_flush);
    assert_sample_count(with_flush, 16000, "process + flush");
    assert!(
        (without_flush as i64 - 16000).abs() > SAMPLE_COUNT_TOLERANCE,
        "expected process() alone to be short of the full count, got {without_flush}"
    );
}

#[test]
fn chunked_processing_matches_one_shot() {
    let input = make_sine(1, 48000, 48000, 220.0);
    let expected = resample(&input, 48000, 16000, None).unwrap();

    let mut resampler = AudioResampler::new(&ResampleOptions::new(48000, 16000)).unwrap();
    let mut chunked: Vec<f32> = Vec::new();
    for chunk in input[0].chunks(1000) {
        let piece = vec![chunk.to_vec()];
        let part = resampler.process(&piece).unwrap();
        if let Some(channel) = part.first() {
            chunked.extend_from_slice(channel);
        }
    }
    if let Some(channel) = resampler.flush().unwrap().first() {
        chunked.extend_from_slice(channel);
    }

    // Filter state carries across chunks, so results match sample for sample.
    assert_eq!(chunked.len(), expected[0].len());
    let max_diff = chunked
        .iter()
        .zip(&expected[0])
        .map(|(a, b)| (a - b).abs())
        .fold(0.0f32, f32::max);
    assert!(max_diff < 1e-6, "max sample difference was {max_diff:e}");
}

#[test]
fn output_rate_and_channels_are_reported() {
    let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000)).unwrap();

    assert_eq!(resampler.output_sample_rate().unwrap(), 16000);
    // Channel count is unknown until the first chunk reveals it.
    assert_eq!(resampler.output_num_channels().unwrap(), 0);

    resampler
        .process(&make_sine(2, 1000, 44100, 440.0))
        .unwrap();
    assert_eq!(resampler.output_num_channels().unwrap(), 2);
}

#[test]
fn empty_input_is_handled() {
    let output = resample(&[], 44100, 16000, None).unwrap();
    assert!(output.is_empty());

    // Flushing before any process() call must not fail.
    let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000)).unwrap();
    assert!(resampler.flush().unwrap().is_empty());
}

#[test]
fn invalid_rates_are_rejected() {
    let input = make_sine(1, 100, 16000, 440.0);

    let error = resample(&input, 0, 16000, None).expect_err("zero input rate");
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);

    let error = resample(&input, 16000, -1, None).expect_err("negative output rate");
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);

    let error = AudioResampler::new(&ResampleOptions::new(16000, 0))
        .expect_err("constructing with a zero output rate");
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
}

#[test]
fn ragged_channels_are_rejected() {
    let ragged = vec![vec![0.0f32; 100], vec![0.0f32; 50]];

    let error = resample(&ragged, 16000, 8000, None).expect_err("ragged channels");

    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
    // Caught in Rust before reaching FFI, so the message names the channel.
    assert!(
        error.message().contains("same length"),
        "unhelpful message: {}",
        error.message()
    );
}

#[test]
fn channel_count_must_not_change_between_calls() {
    let mut resampler = AudioResampler::new(&ResampleOptions::new(16000, 8000)).unwrap();
    resampler.process(&make_sine(2, 100, 16000, 440.0)).unwrap();

    let error = resampler
        .process(&make_sine(1, 100, 16000, 440.0))
        .expect_err("channel count changed mid-stream");

    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
}

#[test]
fn resampler_can_move_between_threads() {
    let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000)).unwrap();

    let total = std::thread::spawn(move || {
        let body = resampler
            .process(&make_sine(1, 44100, 44100, 440.0))
            .unwrap();
        let tail = resampler.flush().unwrap();
        body[0].len() + tail[0].len()
    })
    .join()
    .unwrap();

    assert_sample_count(total, 16000, "resample on another thread");
}
