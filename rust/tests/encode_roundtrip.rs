//! Encoding to disk and reading the result back.

mod common;

use avioflow::{save_audio, AudioDecoder, AudioEncoder, ErrorKind, StreamOptions, WriteOptions};

fn make_ramp(num_channels: usize, num_samples: usize) -> Vec<Vec<f32>> {
    (0..num_channels)
        .map(|c| {
            (0..num_samples)
                .map(|i| {
                    // Distinct per channel so a channel mix-up would show up.
                    let phase = (i as f32 / num_samples as f32) + c as f32 * 0.25;
                    (phase * std::f32::consts::TAU).sin() * 0.5
                })
                .collect()
        })
        .collect()
}

#[test]
fn save_audio_roundtrips_through_wav() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("roundtrip.wav");
    let path_str = path.to_string_lossy().into_owned();

    let samples = make_ramp(2, 16000);
    save_audio(
        &path_str,
        &samples,
        &WriteOptions::new()
            .container_format("wav")
            .codec_name("pcm_s16le")
            .sample_rate(16000)
            .num_channels(2),
    )
    .unwrap();

    assert!(path.exists(), "encoder wrote no file");

    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let metadata = decoder.load_file(&path_str).unwrap();
    let decoded = decoder.get_samples().unwrap();

    assert_eq!(metadata.sample_rate, 16000);
    assert_eq!(metadata.num_channels, 2);
    assert_eq!(decoded.len(), 2);
    assert_eq!(decoded[0].len(), 16000);

    // pcm_s16le quantizes to 1/32768, so compare with that as the bound.
    let max_diff = decoded[0]
        .iter()
        .zip(&samples[0])
        .map(|(a, b)| (a - b).abs())
        .fold(0.0f32, f32::max);
    assert!(
        max_diff < 1e-3,
        "max round-trip difference was {max_diff:e}"
    );
}

#[test]
fn encoder_writes_mono_flac() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("mono.flac");
    let path_str = path.to_string_lossy().into_owned();

    let samples = make_ramp(1, 8000);
    let mut encoder = AudioEncoder::new(
        &WriteOptions::new()
            .container_format("flac")
            .codec_name("flac")
            .sample_rate(8000)
            .num_channels(1),
    )
    .unwrap();
    encoder.save(&path_str, &samples).unwrap();

    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let metadata = decoder.load_file(&path_str).unwrap();

    assert_eq!(metadata.num_channels, 1);
    assert_eq!(metadata.sample_rate, 8000);
    assert_eq!(metadata.codec, "flac");
}

#[test]
fn one_encoder_can_write_several_files() {
    let dir = tempfile::tempdir().unwrap();
    let samples = make_ramp(1, 4000);

    let mut encoder = AudioEncoder::new(
        &WriteOptions::new()
            .container_format("wav")
            .codec_name("pcm_s16le")
            .sample_rate(8000)
            .num_channels(1),
    )
    .unwrap();

    for index in 0..3 {
        let path = dir.path().join(format!("part{index}.wav"));
        encoder.save(&path.to_string_lossy(), &samples).unwrap();
        assert!(path.exists(), "part{index}.wav was not written");
    }
}

#[test]
fn ragged_channels_are_rejected_before_writing() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("ragged.wav");
    let ragged = vec![vec![0.0f32; 100], vec![0.0f32; 50]];

    let error = save_audio(
        &path.to_string_lossy(),
        &ragged,
        &WriteOptions::new()
            .container_format("wav")
            .sample_rate(8000),
    )
    .expect_err("ragged channels should be rejected");

    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
    assert!(!path.exists(), "no file should be created on failure");
}

#[test]
fn unwritable_path_reports_an_error() {
    let samples = make_ramp(1, 100);

    let error = save_audio(
        "/nonexistent-directory/out.wav",
        &samples,
        &WriteOptions::new()
            .container_format("wav")
            .sample_rate(8000),
    )
    .expect_err("writing into a missing directory should fail");

    assert!(
        !error.message().is_empty(),
        "error carried no message from the native layer"
    );
}
