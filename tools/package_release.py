#!/usr/bin/env python3
"""Package a built out/game release without shipping extracted disc or user data.

Usage: python tools/package_release.py [--version VERSION]
Run build.cmd package for the normal build + packaging workflow.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "out" / "game"
DIST = ROOT / "out" / "dist"
SDK = ROOT / "out" / "tooling" / "runtime-sdk" / "win-amd64"

# Keep this an explicit allowlist. Never walk out/game recursively: it holds
# retail disc content, saves, caches, and user-edited configuration.
REQUIRED_BINARIES = (
    "fable_2.exe",
    "Fable2Launcher.exe",
    "fable2_build.json",
    "rexruntime.dll",
    "rexgpu-xenos.dll",
)
OPTIONAL_LAUNCH_FILES = (
    "fable2.cmd",
    "fable2-uncapped.cmd",
    "fable2-functrace.cmd",
    "StartWithoutAudio.cmd",
    "launch_renderer.ps1",
    "app-icon.png",
)


def require(path: Path) -> Path:
    if not path.is_file() or path.is_symlink():
        raise SystemExit(f"package_release: required regular file missing: {path}")
    return path


def add(files: dict[str, Path], name: str, path: Path) -> None:
    # Hard-fail duplicate destinations instead of silently replacing a file.
    if name in files:
        raise SystemExit(f"package_release: duplicate archive entry: {name}")
    files[name] = require(path)


def gather_player_files(root: Path = ROOT, game: Path = GAME) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for name in REQUIRED_BINARIES:
        add(files, name, game / name)
    for name in OPTIONAL_LAUNCH_FILES:
        if (game / name).is_file():
            add(files, name, game / name)

    # Ship the canonical defaults, never the player's edited out/game copy.
    add(files, "fable2_config.toml", root / "config" / "fable2_config.toml")
    add(files, "README.md", root / "README.md")

    # Lua here belongs to the project; out/game/data also contains retail files.
    scripts = sorted((root / "src" / "lua").glob("*.lua"))
    if not scripts:
        raise SystemExit("package_release: no authored recomp Lua scripts")
    for path in scripts:
        add(files, f"data/scripts/recomp/{path.name}", path)

    # Original HLSL replacements from graphics PR #51 must ship with the
    # source-built runtime that loads them. Read only project-owned compiled
    # DXBC variants; never recurse through the user's extracted game folder.
    materials_dir = root / "materials" / "d3d12"
    materials = sorted(materials_dir.glob("*.dxbc"))
    if not materials:
        raise SystemExit("package_release: compiled material shader variants missing")
    for path in materials:
        add(files, f"materials/d3d12/{path.name}", path)

    # The renderer selector requires a matched, validated D3D12 pair. The
    # optional Vulkan pair is included only when its complete manifest exists.
    for backend in ("d3d12", "vulkan"):
        folder = game / "renderers" / backend
        manifest = folder / "renderer-pair.json"
        if backend == "vulkan" and not manifest.exists():
            continue
        for filename in ("renderer-pair.json", "rexruntime.dll", "rexgpu-xenos.dll"):
            add(files, f"renderers/{backend}/{filename}", folder / filename)
        loader = folder / "vulkan-1.dll"
        if backend == "vulkan" and loader.is_file():
            add(files, f"renderers/{backend}/vulkan-1.dll", loader)
    return files


def gather_symbols(game: Path = GAME, sdk: Path = SDK) -> dict[str, Path]:
    files: dict[str, Path] = {}
    add(files, "fable_2.pdb", game / "fable_2.pdb")
    for name in ("rexruntime.pdb", "rexgpu-xenos.pdb"):
        path = sdk / "bin" / name
        if path.is_file():
            add(files, f"d3d12/{name}", path)
    return files


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def atomic_zip(path: Path, folder: str, sources: dict[str, Path]) -> None:
    fd, temp = tempfile.mkstemp(prefix=".fable2-", suffix=".zip", dir=path.parent)
    os.close(fd)
    try:
        with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for name, src in sorted(sources.items()):
                z.write(src, f"{folder}/{name}")
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def version_label(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+-]*", value):
        raise SystemExit("package_release: version contains unsafe filename characters")
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", help="Override git describe --tags --always")
    args = parser.parse_args()
    version = version_label(args.version or subprocess.check_output(
        ["git", "-C", str(ROOT), "describe", "--tags", "--always"], text=True
    ).strip())
    player = gather_player_files()
    symbols = gather_symbols()
    DIST.mkdir(parents=True, exist_ok=True)

    player_zip = DIST / f"Fable_2_recomp_v{version}_windows.zip"
    symbol_zip = DIST / f"Fable_2_recomp_v{version}_symbols.zip"
    atomic_zip(player_zip, f"Fable 2 {version}", player)
    atomic_zip(symbol_zip, f"Fable 2 {version} symbols", symbols)
    sums = "".join(f"{sha256(path)}  {path.name}\n" for path in (player_zip, symbol_zip))
    checksum_path = DIST / "SHA256SUMS"
    fd, temp = tempfile.mkstemp(prefix=".sha256-", dir=DIST, text=True)
    try:
        with os.fdopen(fd, "w", encoding="ascii", newline="\n") as stream:
            stream.write(sums)
        os.replace(temp, checksum_path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)

    print(f"Player ZIP: {player_zip}")
    print(f"Symbols ZIP: {symbol_zip}")
    print(f"SHA-256 checksums: {checksum_path}")
    print("No game dumps, disc manifest, saves, logs or personal configuration included.")


if __name__ == "__main__":
    main()
