"""Validate and fingerprint an installed source-built ReXGlue SDK.

Usage: python tools/stage_renderer_sdk.py SDK_SOURCE BUILD_DIR OUTPUT
       [--configuration Release|Debug]

The SDK itself is installed into OUTPUT by CMake before this script runs. This
script refuses mixed or incomplete packages and writes renderer provenance.
Release packages must include the source-built rexglue.exe used for game
codegen, so codegen/runtime/plugin all come from the same SDK revision.
"""
import argparse
import json
from pathlib import Path
import shutil

from renderer_provenance import build_identity, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "output"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    args = parser.parse_args()
    source, build, output = (
        getattr(args, name).resolve() for name in ("source", "build", "output")
    )
    if any(
        output == path or path.is_relative_to(output) or output.is_relative_to(path)
        for path in (source, build)
    ):
        raise SystemExit("Output must be separate from every input tree")

    native = source / "out/win-amd64"
    suffix = "d" if args.configuration == "Debug" else ""
    runtime_name = f"rexruntime{suffix}.dll"
    plugin_name = f"rexgpu-xenos{suffix}.dll"
    runtime = native / runtime_name
    plugin = native / plugin_name
    staged_runtime = output / "bin" / runtime_name
    staged_plugin = output / "bin" / plugin_name

    required = [
        runtime,
        plugin,
        native / f"rexruntime{suffix}.lib",
        native / f"rexgpu-xenos{suffix}.lib",
        build / "include/rex/version.h",
        output / "include/rex/version.h",
        output / "lib/cmake/rexglue/rexglueConfig.cmake",
        staged_runtime,
        staged_plugin,
    ]
    if args.configuration == "Release":
        required.extend((native / "rexglue.exe", output / "bin/rexglue.exe"))

    for path in required:
        if not path.is_file():
            raise SystemExit(f"Missing source-built SDK artifact: {path}")

    if digest(runtime) != digest(staged_runtime):
        raise SystemExit("Installed ReXGlue runtime does not match the source build")
    if digest(plugin) != digest(staged_plugin):
        raise SystemExit("Installed ReXGlue GPU plugin does not match the source build")

    # CMake's target install does not consistently carry Clang/lld PDBs.
    # Preserve source-built symbols beside the corresponding staged binaries.
    staged_bin = output / "bin"
    for name in (
        f"rexruntime{suffix}.pdb",
        f"rexgpu-xenos{suffix}.pdb",
        "rexglue.pdb",
    ):
        source_pdb = native / name
        if source_pdb.is_file():
            shutil.copy2(source_pdb, staged_bin / name)

    provenance = build_identity(
        source, build, "d3d12", args.configuration, runtime, plugin
    )
    codegen = output / "bin/rexglue.exe"
    if codegen.is_file():
        source_codegen = native / "rexglue.exe"
        if source_codegen.is_file() and digest(source_codegen) != digest(codegen):
            raise SystemExit("Installed rexglue.exe does not match the source build")
        provenance["codegen_tool"] = str(codegen)
        provenance["codegen_sha256"] = digest(codegen)
        provenance["note"] = (
            "Headers, runtime, GPU plugin and codegen tool are installed from "
            "the same source-built SDK."
        )
    else:
        provenance["codegen_tool"] = None
        provenance["note"] = (
            "Debug runtime package; game codegen should use the matched Release "
            "source-built rexglue tool."
        )

    (output / "renderer-sdk-provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Verified source-built SDK at {output}")
    if codegen.is_file():
        print(f"Codegen tool: {codegen}")


if __name__ == "__main__":
    main()
