import io
import subprocess
import sys
import textwrap


def test_save_memory_targets_do_not_crash():
    script = textwrap.dedent(
        """
        import io
        import avioflow
        import numpy as np

        samples = np.zeros((1, 8000), dtype=np.float32)
        options = avioflow.AudioWriteOptions("flac", sample_rate=8000)

        byte_buffer = bytearray()
        assert avioflow.save(byte_buffer, samples, options) is None
        assert byte_buffer.startswith(b"fLaC")

        stream = io.BytesIO()
        assert avioflow.save(stream, samples, options) is None
        assert stream.getvalue().startswith(b"fLaC")

        metadata, decoded = avioflow.load(bytes(byte_buffer))
        assert metadata.sample_rate == 8000
        assert decoded.shape == samples.shape
        """
    )
    completed = subprocess.run(
        [sys.executable, "-c", script],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert completed.returncode == 0, completed.stderr


def test_save_bytearray_defaults_to_wav():
    import avioflow
    import numpy as np

    samples = np.zeros((1, 8000), dtype=np.float32)
    options = avioflow.AudioWriteOptions("wav", sample_rate=8000)
    target = bytearray(b"old contents")

    assert avioflow.save(target, samples, options) is None
    assert target.startswith(b"RIFF")
    assert target[8:12] == b"WAVE"
