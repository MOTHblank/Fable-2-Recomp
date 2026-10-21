"""Regression checks for the ReXGlue entrypoint manifest and single game root."""
import tempfile
import unittest
from pathlib import Path

from prepare_codegen_manifest import prepare


class PrepareManifestTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.root = Path(self.tempdir.name)
        (self.root / "out/game").mkdir(parents=True)
        (self.root / "out/game/default.xex").write_bytes(b"fixture")
        self.source = self.root / "fable_2_manifest.toml"
        self.generated = self.root / ".fable2_codegen_manifest.toml"

    def test_rewrites_entrypoint_only_and_preserves_original(self):
        original = (
            b'# original manifest\n'
            b'[project]\nname = "fable_2"\ngame_root = "."\n'
            b'[entrypoint]\nfile_path = "default.xex" # disc\n'
            b'out_directory_path = "generated/default"\n'
            b'[[entrypoint.midasm_hook]]\nfile_path = "unrelated.xex"\n'
        )
        self.source.write_bytes(original)
        prepare(self.source, self.generated, "out/game/default.xex")
        updated = self.generated.read_bytes()
        self.assertEqual(original, self.source.read_bytes())
        self.assertIn(b'file_path = "out/game/default.xex" # disc', updated)
        self.assertIn(b'file_path = "unrelated.xex"', updated)
        self.assertIn(b'game_root = "."', updated)
        self.assertIn(b'out_directory_path = "generated/default"', updated)
        modified = self.generated.stat().st_mtime_ns
        prepare(self.source, self.generated, "out/game/default.xex")
        self.assertEqual(modified, self.generated.stat().st_mtime_ns)

    def test_requires_entrypoint_file_path(self):
        self.source.write_text('[project]\nfile_path = "default.xex"\n[entrypoint]\nname="game"\n')
        with self.assertRaisesRegex(ValueError, "file_path under"):
            prepare(self.source, self.generated, "out/game/default.xex")

    def test_missing_xex_fails_cleanly(self):
        self.source.write_text('[entrypoint]\nfile_path = "default.xex"\n')
        (self.root / "out/game/default.xex").unlink()
        with self.assertRaisesRegex(ValueError, "Missing"):
            prepare(self.source, self.generated, "out/game/default.xex")


if __name__ == "__main__":
    unittest.main()
