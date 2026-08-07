//! Decoding a file from disk: metadata, whole-file samples, time ranges,
//! zero-copy frames, and error reporting.

mod common;

use avioflow::{AudioDecoder, ErrorKind, StreamOptions};
use common::{assert_sample_count, test_audio_path, NUM_CHANNELS, NUM_SAMPLES, SAMPLE_RATE};

fn path() -> String {
    test_audio_path().to_string_lossy().into_owned()
}

#[test]
fn load_file_reports_metadata() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let metadata = decoder.load_file(&path()).unwrap();

    assert_eq!(metadata.sample_rate, SAMPLE_RATE);
    assert_eq!(metadata.num_channels, NUM_CHANNELS as i32);
    // The decoder name, not the container's: FFmpeg decodes mp3 with mp3float.
    assert_eq!(metadata.codec, "mp3float");
    assert_eq!(metadata.container, "mp3");
    assert!(
        metadata.duration > 97.0 && metadata.duration < 98.0,
        "duration was {}",
        metadata.duration
    );
}

#[test]
fn get_samples_returns_every_sample() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let samples = decoder.get_samples().unwrap();

    assert_eq!(samples.len(), NUM_CHANNELS);
    assert_eq!(samples[0].len(), NUM_SAMPLES);
    assert_eq!(samples[1].len(), NUM_SAMPLES);
    assert!(decoder.is_finished().unwrap());

    // Real audio, not a buffer of silence or garbage.
    let peak = samples[0].iter().fold(0.0f32, |acc, s| acc.max(s.abs()));
    assert!(peak > 0.01 && peak <= 2.0, "peak amplitude was {peak}");
    assert!(samples[0].iter().all(|s| s.is_finite()));
}

#[test]
fn decoder_resamples_when_asked() {
    let target_rate = 16000;
    let mut decoder =
        AudioDecoder::new(&StreamOptions::new().output_sample_rate(target_rate)).unwrap();
    let metadata = decoder.load_file(&path()).unwrap();

    // load_file reports the source stream: the resampler is not set up until the
    // first frame is decoded, so the output rate is not visible yet.
    assert_eq!(metadata.sample_rate, SAMPLE_RATE);

    let samples = decoder.get_samples().unwrap();
    let expected = NUM_SAMPLES as i64 * target_rate as i64 / SAMPLE_RATE as i64;

    assert_eq!(samples.len(), NUM_CHANNELS);
    // Exercises the EOF resampler drain: without it the tail is short by ~16.
    assert_sample_count(samples[0].len(), expected, "decoder resample to 16 kHz");

    // After decoding, metadata reflects the resampled output.
    assert_eq!(decoder.metadata().unwrap().sample_rate, target_rate);
}

#[test]
fn decoder_downmixes_to_mono() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new().output_num_channels(1)).unwrap();
    decoder.load_file(&path()).unwrap();

    let samples = decoder.get_samples().unwrap();

    assert_eq!(samples.len(), 1);
    assert_eq!(samples[0].len(), NUM_SAMPLES);
}

#[test]
fn get_samples_range_decodes_only_the_requested_window() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let samples = decoder.get_samples_range(10.0, Some(20.0)).unwrap();

    let expected = 10 * SAMPLE_RATE as i64;
    assert_eq!(samples.len(), NUM_CHANNELS);
    // Range edges land on the enclosing frame, so allow a frame of slack rather
    // than the tight resampling tolerance.
    let diff = samples[0].len() as i64 - expected;
    assert!(
        diff.abs() <= 2304,
        "expected ~{expected} samples for a 10s window, got {} (diff {diff})",
        samples[0].len()
    );
}

/// Ranges late in the file are a regression guard. The trim bounds are absolute
/// sample offsets, but a seek restarts the decoder's own sample counter, so
/// comparing the two directly made every range past roughly half the duration
/// come back short or empty.
#[test]
fn ranges_work_across_the_whole_file() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let expected = 5 * SAMPLE_RATE as i64;
    for start in [0.0, 20.0, 50.0, 70.0, 90.0] {
        let samples = decoder.get_samples_range(start, Some(start + 5.0)).unwrap();
        assert!(
            !samples.is_empty() && !samples[0].is_empty(),
            "range starting at {start}s returned nothing"
        );
        let diff = samples[0].len() as i64 - expected;
        assert!(
            diff.abs() <= 2304,
            "range starting at {start}s: expected ~{expected} samples, got {} (diff {diff})",
            samples[0].len()
        );
    }
}

/// An unset stop must decode to the end from any start point.
#[test]
fn open_ended_ranges_reach_the_end() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    // TownTheme.mp3 is ~97.45s.
    for (start, expected_seconds) in [(0.0, 97.45), (30.0, 67.45), (90.0, 7.45)] {
        let samples = decoder.get_samples_range(start, None).unwrap();
        assert!(
            !samples.is_empty() && !samples[0].is_empty(),
            "open-ended range from {start}s returned nothing"
        );
        let seconds = samples[0].len() as f64 / SAMPLE_RATE as f64;
        assert!(
            (seconds - expected_seconds).abs() < 0.1,
            "from {start}s: got {seconds:.3}s, expected ~{expected_seconds}s"
        );
    }
}

#[test]
fn ranges_can_be_requested_repeatedly_from_one_decoder() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let first = decoder.get_samples_range(5.0, Some(6.0)).unwrap();
    let second = decoder.get_samples_range(5.0, Some(6.0)).unwrap();
    let elsewhere = decoder.get_samples_range(30.0, Some(31.0)).unwrap();

    // Each call seeks independently, so the same range yields the same audio.
    assert_eq!(first[0].len(), second[0].len());
    assert_eq!(first[0], second[0]);
    assert_ne!(first[0], elsewhere[0]);
}

#[test]
fn get_frame_walks_the_stream_without_copying() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let mut frames = 0usize;
    let mut total = 0usize;
    let mut channels = 0usize;

    while let Some(frame) = decoder.get_frame().unwrap() {
        frames += 1;
        total += frame.num_samples();
        channels = frame.num_channels();
        assert_eq!(frame.channel(0).map(|c| c.len()), Some(frame.num_samples()));
        assert!(frame.channel(frame.num_channels()).is_none());
    }

    assert!(frames > 0, "decoded no frames");
    assert_eq!(channels, NUM_CHANNELS);
    assert_eq!(total, NUM_SAMPLES);
}

#[test]
fn load_buffer_matches_load_file() {
    let bytes = std::fs::read(test_audio_path()).unwrap();

    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let metadata = decoder.load_buffer(&bytes).unwrap();
    let samples = decoder.get_samples().unwrap();

    assert_eq!(metadata.sample_rate, SAMPLE_RATE);
    assert_eq!(samples.len(), NUM_CHANNELS);
    assert_eq!(samples[0].len(), NUM_SAMPLES);
}

#[test]
fn missing_file_reports_a_message() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let error = decoder
        .load_file("/nonexistent/definitely-not-here.mp3")
        .expect_err("loading a missing file should fail");

    // The point is that avf_last_error() is wired through, not the exact text.
    assert!(
        !error.message().is_empty(),
        "error carried no message from the native layer"
    );
    assert_eq!(error.kind(), ErrorKind::Runtime);
}

#[test]
fn interior_nul_in_path_is_rejected_before_ffi() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    let error = decoder
        .load_file("bad\0path.mp3")
        .expect_err("a path with an interior NUL cannot be passed to C");

    assert_eq!(error.kind(), ErrorKind::InvalidString);
}

#[test]
fn invalid_range_is_rejected() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let error = decoder
        .get_samples_range(20.0, Some(10.0))
        .expect_err("stop before start should fail");
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);

    let error = decoder
        .get_samples_range(-1.0, None)
        .expect_err("negative start should fail");
    assert_eq!(error.kind(), ErrorKind::InvalidArgument);
}

#[test]
fn decoder_can_move_between_threads() {
    let mut decoder = AudioDecoder::new(&StreamOptions::new()).unwrap();
    decoder.load_file(&path()).unwrap();

    let samples = std::thread::spawn(move || decoder.get_samples().unwrap())
        .join()
        .unwrap();

    assert_eq!(samples[0].len(), NUM_SAMPLES);
}

#[test]
fn format_queries_return_known_entries() {
    let decoders = avioflow::supported_decoders().unwrap();
    let encoders = avioflow::supported_encoders().unwrap();
    let inputs = avioflow::supported_input_formats().unwrap();
    let outputs = avioflow::supported_output_formats().unwrap();

    assert!(decoders.iter().any(|d| d == "mp3"), "no mp3 decoder");
    assert!(
        encoders.iter().any(|e| e == "pcm_s16le"),
        "no pcm_s16le encoder"
    );
    assert!(inputs.iter().any(|f| f == "mp3"), "no mp3 demuxer");
    assert!(outputs.iter().any(|f| f == "wav"), "no wav muxer");
}

#[test]
fn set_log_level_accepts_levels_and_reset() {
    // No return value to check; this asserts the calls are safe and that a
    // pathological input takes the ignore path rather than panicking.
    avioflow::set_log_level(Some("quiet"));
    avioflow::set_log_level(Some("bogus-level"));
    avioflow::set_log_level(Some("with\0nul"));
    avioflow::set_log_level(None);
}
