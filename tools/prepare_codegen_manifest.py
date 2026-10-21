#!/usr/bin/env python3
"""Prepare a sibling ReXGlue manifest for the fixed out/game input location.

The tracked, multi-megabyte source manifest stays unchanged. Only the [entrypoint].file_path key changes, and only when bytes differ.
"""
from pathlib import Path
import os
import re
import sys
import tempfile


def prepare(source: Path, destination: Path, xex_relative: str) -> None:
    source = source.resolve()
    destination = destination.resolve()
    if destination.parent != source.parent:
        raise ValueError("The generated manifest must sit next to the source manifest.")
    xex = source.parent / Path(xex_relative)
    if not xex.is_file():
        raise ValueError(f"Missing {xex}. Use build.cmd to extract a supported ISO first.")

    content = source.read_bytes()
    # [entrypoint].file_path is a TOML table member, not a root-level key.
    # Do not modify unrelated keys under later [entrypoint.*] nested tables.
    header = re.search(rb'(?m)^[ \t]*\[entrypoint\][ \t]*(?:#.*)?\r?$', content)
    if header is None:
        raise ValueError("Missing [entrypoint] table in the original ReXGlue manifest.")
    start = header.end()
    next_table = re.search(rb'(?m)^[ \t]*\[', content[start:])
    end = start + next_table.start() if next_table else len(content)
    match = re.search(
        rb'(?m)^([ \t]*file_path[ \t]*=[ \t]*)(["\x27])[^\r\n]*?\2(?=[ \t]*(?:#.*)?\r?$)',
        content[start:end],
    )
    if match is None:
        raise ValueError("Missing file_path under [entrypoint] in the ReXGlue manifest.")

    # ReXGlue resolves paths relative to the manifest's directory.
    value = xex_relative.replace("\\", "/").encode("utf-8")
    replacement = match.group(1) + b'"' + value + b'"'
    absolute_start = start + match.start()
    absolute_end = start + match.end()
    updated = content[:absolute_start] + replacement + content[absolute_end:]
    if destination.exists() and destination.read_bytes() == updated:
        return

    fd, temporary = tempfile.mkstemp(prefix=".fable2-manifest-", dir=destination.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(updated)
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("Usage: prepare_codegen_manifest.py <source> <destination> <xex-relative>")
    try:
        prepare(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3])
    except (ValueError, OSError) as exc:
        sys.exit(f"Codegen manifest preparation failed: {exc}")
