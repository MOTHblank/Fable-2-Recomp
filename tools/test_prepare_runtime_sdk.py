"""Non-destructive checks for the integrated ReXGlue runtime fixes."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from prepare_runtime_sdk import (
    BAKED_RUNTIME_MARKERS,
    BAKED_SDK_PIN,
    missing_baked_runtime_features,
)


class BakedSdkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.sdk = Path(self.directory.name)
        for relative, markers in BAKED_RUNTIME_MARKERS.items():
            file = self.sdk / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("\n".join(markers) + "\n", encoding="utf-8")

    @patch("prepare_runtime_sdk.libmspack_mode", return_value="040000")
    def test_complete_integrated_fork(self, _mode):
        self.assertEqual(len(BAKED_SDK_PIN), 40)
        self.assertEqual(missing_baked_runtime_features(self.sdk), [])

    @patch("prepare_runtime_sdk.libmspack_mode", return_value="040000")
    def test_missing_source_is_rejected(self, _mode):
        missing = "include/rex/audio/clocked_audio_sink.h"
        (self.sdk / missing).unlink()
        self.assertTrue(any(missing in item for item in missing_baked_runtime_features(self.sdk)))

    @patch("prepare_runtime_sdk.libmspack_mode", return_value="040000")
    def test_incomplete_worktree_is_rejected(self, _mode):
        file = self.sdk / "src/graphics/command_processor.cpp"
        file.write_text("custom local edit", encoding="utf-8")
        self.assertTrue(any("frame_limiter_.Pace" in item
                            for item in missing_baked_runtime_features(self.sdk)))

    @patch("prepare_runtime_sdk.libmspack_mode", return_value="160000")
    def test_old_libmspack_gitlink_does_not_count_as_vendored(self, _mode):
        self.assertTrue(any("thirdparty/libmspack" in item
                            for item in missing_baked_runtime_features(self.sdk)))


if __name__ == "__main__":
    unittest.main()
