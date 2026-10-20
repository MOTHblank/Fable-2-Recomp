"""Snapshot a complete Vulkan runtime/plugin pair before any D3D12 rebuild.

The SDK's native output directory is shared between backends. This snapshot
is the only Vulkan input accepted by the game staging step.
"""
import argparse
import json
from pathlib import Path
import shutil
from renderer_provenance import build_identity


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", type=Path)
    ap.add_argument("build", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    source, build, output = (x.resolve() for x in
                             (args.source, args.build, args.output))
    native = source / "out/win-amd64/Release"
    runtime, plugin = native / "rexruntime.dll", native / "rexgpu-xenos.dll"
    for path in (runtime, plugin):
        if not path.is_file():
            raise SystemExit(f"Vulkan build output missing: {path}")
    identity = build_identity(source, build, "vulkan", "Release", runtime, plugin)
    bin_dir = output / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    for path in (runtime, plugin):
        shutil.copy2(path, bin_dir / path.name)
    for name in ("rexruntime.pdb", "rexgpu-xenos.pdb"):
        if (native / name).is_file():
            shutil.copy2(native / name, bin_dir / name)
    from renderer_provenance import digest
    if digest(bin_dir / runtime.name) != identity["runtime_sha256"] or (
        digest(bin_dir / plugin.name) != identity["plugin_sha256"]
    ):
        raise SystemExit("Vulkan staging checksum mismatch")
    (output / "renderer-sdk-provenance.json").write_text(
        json.dumps(identity, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Vulkan SDK pair saved to {output}")


if __name__ == "__main__":
    main()
