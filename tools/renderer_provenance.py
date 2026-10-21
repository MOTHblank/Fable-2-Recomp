"""Common build identity for the two Windows renderer SDK builds.

The SDK checkout is the gitlink at thirdparty/rexglue-sdk. Both builders call
prepare_runtime_sdk.py before compiling; its tracked patches define the
additional source inputs, regardless of whether a fork already embeds them.
"""
import hashlib
from pathlib import Path
import subprocess

PATCHES = ("rexglue-sdk-runtime-fixes.patch", "rexglue-sdk-debug-exports.patch")


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def patch_digest():
    root = Path(__file__).resolve().parents[1] / "thirdparty"
    h = hashlib.sha256()
    for name in PATCHES:
        h.update(name.encode("utf-8") + b"\0")
        h.update((root / name).read_bytes())
    return h.hexdigest()


def git_commit(source):
    return subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()


def build_identity(source, build, backend, configuration, runtime, plugin):
    cache = Path(build) / "CMakeCache.txt"
    if not cache.is_file():
        raise SystemExit(f"Missing SDK build cache: {cache}")
    values = {}
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" in line and ":" in line.split("=", 1)[0] and not line.startswith(("//", "#")):
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    compiler = values.get("CMAKE_CXX_COMPILER", "")
    if not compiler:
        raise SystemExit(f"No C++ compiler recorded by {cache}")
    version = subprocess.check_output(
        [compiler, "--version"], text=True, errors="replace"
    ).splitlines()[0].strip()
    # Capture any applied working-tree patch (including local SDK changes),
    # rather than relying on a HEAD SHA that doesn't change after git apply.
    diff = subprocess.check_output(
        ["git", "-C", str(source), "diff", "--binary", "HEAD"]
    )
    return {
        "worktree_diff_sha256": hashlib.sha256(diff).hexdigest(),
        "sdk_use_d3d12": values.get("REXGLUE_USE_D3D12", "").upper(),
        "sdk_use_vulkan": values.get("REXGLUE_USE_VULKAN", "").upper(),
        "schema": 1,
        "backend": backend,
        "configuration": configuration,
        "sdk_commit": git_commit(source),
        "patch_sha256": patch_digest(),
        "compiler": Path(compiler).name.lower(),
        "compiler_version": version,
        "cmake_generator": values.get("CMAKE_GENERATOR", ""),
        "cmake_cxx_flags": values.get("CMAKE_CXX_FLAGS", ""),
        "cmake_build_type": values.get("CMAKE_BUILD_TYPE", ""),
        "runtime_sha256": digest(runtime),
        "plugin_sha256": digest(plugin),
    }
