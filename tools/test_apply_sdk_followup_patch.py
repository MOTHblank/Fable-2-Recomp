"""Validate exact SDK patch recovery without a checkout or network access."""
import tempfile
import unittest
from pathlib import Path

from apply_sdk_followup_patch import apply_exact_hunks


class ExactSdkPatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "src").mkdir()
        self.cpp = self.root / "src/graphics.cpp"
        self.patch = self.root / "fix.patch"
        self.patch.write_text(
            "diff --git a/src/graphics.cpp b/src/graphics.cpp\n"
            "--- a/src/graphics.cpp\n+++ b/src/graphics.cpp\n"
            "@@ -1,3 +1,3 @@\n"
            " int before;\n-old();\n+new_call();\n int after;\n"
            "@@ -6,3 +6,3 @@\n"
            " int middle;\n-queue_old();\n+queue_new();\n int end;\n"
        )
        self.original = (
            "int before;\nold();\nint after;\nint blank;\n"
            "int middle;\nqueue_old();\nint end;\n"
        )

    def test_applies_and_is_idempotent(self):
        self.cpp.write_text(self.original)
        self.assertEqual(apply_exact_hunks(self.root, self.patch), "applied")
        self.assertIn("new_call();", self.cpp.read_text())
        self.assertIn("queue_new();", self.cpp.read_text())
        self.assertEqual(apply_exact_hunks(self.root, self.patch), "already applied")

    def test_resumes_partial_and_preserves_unrelated_edits(self):
        current = self.original.replace("\nold();\n", "\nnew_call();\n").replace(
            "int blank;", "int unrelated_edit;")
        self.cpp.write_text(current)
        self.assertEqual(apply_exact_hunks(self.root, self.patch), "applied")
        self.assertIn("int unrelated_edit;", self.cpp.read_text())
        self.assertIn("queue_new();", self.cpp.read_text())

    def test_unknown_hunk_prevents_any_write(self):
        current = self.original.replace("queue_old();", "custom_local_change();")
        self.cpp.write_text(current)
        with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
            apply_exact_hunks(self.root, self.patch)
        self.assertEqual(self.cpp.read_text(), current)

    def test_preserves_crlf(self):
        self.cpp.write_bytes(self.original.replace("\n", "\r\n").encode())
        self.assertEqual(apply_exact_hunks(self.root, self.patch), "applied")
        raw = self.cpp.read_bytes()
        self.assertEqual(raw.count(b"\n"), raw.count(b"\r\n"))


if __name__ == "__main__":
    unittest.main()
