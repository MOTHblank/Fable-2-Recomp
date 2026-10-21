"""Strict recovery for partially applied ReXGlue follow-up patches.

Never fuzz, revert, or overwrite unrecognized SDK changes. Each unified-diff
hunk must match the *entire* old or new text exactly, once, including context.
This handles partially applied patches and Git apply offset/context conflicts.
"""
from pathlib import Path
import os
import re
import stat
import tempfile


HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")
DIFF = re.compile(r"^diff --git a/(.+) b/(.+)$")


def _hunks(patch: Path):
    lines = patch.read_text(encoding="utf-8").splitlines()
    result = {}
    file_name = None
    i = 0
    while i < len(lines):
        line = lines[i]
        diff = DIFF.fullmatch(line)
        if diff:
            if diff.group(1) != diff.group(2):
                raise ValueError("Only in-place file modifications are supported")
            file_name = diff.group(1)
            result.setdefault(file_name, [])
            i += 1
            continue
        header = HUNK.match(line)
        if header:
            if file_name is None:
                raise ValueError("Hunk appears before file header")
            old_size = int(header.group(2) or 1)
            new_size = int(header.group(4) or 1)
            if not old_size or not new_size:
                raise ValueError("File creation/deletion hunks are not supported")
            before, after = [], []
            i += 1
            while len(before) < old_size or len(after) < new_size:
                if i >= len(lines):
                    raise ValueError("Truncated patch hunk")
                item = lines[i]
                if not item or item[0] not in " +-":
                    raise ValueError(f"Unsupported or malformed patch hunk: {item!r}")
                if item[0] != "+":
                    before.append(item[1:])
                if item[0] != "-":
                    after.append(item[1:])
                if len(before) > old_size or len(after) > new_size:
                    raise ValueError("Hunk line count exceeds its header")
                i += 1
            result[file_name].append(("\n".join(before) + "\n",
                                      "\n".join(after) + "\n"))
            continue
        i += 1
    if not result or any(not hunks for hunks in result.values()):
        raise ValueError("Expected modified-file hunks")
    return result


def apply_exact_hunks(source: Path, patch: Path) -> str:
    """Return 'applied' or 'already applied'. Fail closed on any ambiguity.

    All files/hunks are verified in memory before the first write, ensuring an
    incompatible later hunk cannot result in a partially updated patch.
    """
    source = source.resolve()
    staged = {}
    applied_count = 0
    for relative, hunks in _hunks(patch).items():
        path = (source / relative).resolve()
        if not path.is_relative_to(source) or not path.is_file():
            raise ValueError(f"Unsafe or missing SDK patch path: {relative}")
        raw = path.read_bytes()
        crlf = raw.count(b"\r\n")
        if crlf and crlf != raw.count(b"\n"):
            raise ValueError(f"Mixed line endings in {relative}")
        newline = "\r\n" if crlf else "\n"
        try:
            contents = raw.decode("utf-8").replace("\r\n", "\n")
        except UnicodeDecodeError as exc:
            raise ValueError(f"Non-UTF8 SDK source: {relative}") from exc
        for number, (before, after) in enumerate(hunks, 1):
            if before == after:
                continue
            old_count, new_count = contents.count(before), contents.count(after)
            if old_count == 1 and new_count == 0:
                contents = contents.replace(before, after, 1)
                applied_count += 1
            elif new_count == 1 and old_count == 0:
                continue
            else:
                raise ValueError(
                    f"{relative}: hunk {number} does not match exactly "
                    f"(old={old_count}, new={new_count}); refusing to overwrite local edits")
        updated = contents.replace("\n", newline).encode("utf-8")
        if updated != raw:
            staged[path] = updated

    for path, updated in staged.items():
        temporary = None
        try:
            fd, name = tempfile.mkstemp(dir=path.parent, prefix=".fable2-sdk-patch-")
            temporary = Path(name)
            with os.fdopen(fd, "wb") as handle:
                handle.write(updated)
            temporary.chmod(stat.S_IMODE(path.stat().st_mode))
            os.replace(temporary, path)
        finally:
            if temporary is not None and temporary.exists():
                temporary.unlink()
    return "applied" if applied_count else "already applied"
