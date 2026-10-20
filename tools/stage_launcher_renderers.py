"""Stage checked, backend-specific runtime/plugin pairs next to a Release exe.

No source/out DLLs are used: only the independently snapshotted SDK pairs.
A missing optional Vulkan pair never breaks a successful D3D12 build.
"""
import argparse
import json
from pathlib import Path
import shutil
from renderer_provenance import digest

REQUIRED = ("sdk_commit", "patch_sha256", "worktree_diff_sha256",
            "compiler", "compiler_version",
            "configuration", "runtime_sha256", "plugin_sha256", "backend")


def checked_source(root, backend):
    meta_path = root / "renderer-sdk-provenance.json"
    if not meta_path.is_file():
        raise ValueError(f"provenance missing: {meta_path}")
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    for field in REQUIRED:
        if not meta.get(field):
            raise ValueError(f"{meta_path} missing {field}")
    if meta["backend"] != backend or meta["configuration"] != "Release":
        raise ValueError(f"{backend}: wrong backend/configuration in {meta_path}")
    if meta.get("sdk_use_d3d12") not in ("ON", "TRUE", "1") or (
        backend == "vulkan" and meta.get("sdk_use_vulkan") not in ("ON", "TRUE", "1")
    ):
        raise ValueError(f"{backend}: SDK was not built with the required renderer")
    runtime, plugin = (root / "bin" / name for name in
                       ("rexruntime.dll", "rexgpu-xenos.dll"))
    for path, field in ((runtime, "runtime_sha256"), (plugin, "plugin_sha256")):
        if not path.is_file() or digest(path) != meta[field]:
            raise ValueError(f"{backend}: missing or mismatched {path}")
    return meta, runtime, plugin


def stage(dest, backend, metadata, runtime, plugin, loader=None):
    target = dest / "renderers" / backend
    target.mkdir(parents=True, exist_ok=True)
    for path in (runtime, plugin):
        shutil.copy2(path, target / path.name)
    for name, expected in (("rexruntime.dll", metadata["runtime_sha256"]),
                           ("rexgpu-xenos.dll", metadata["plugin_sha256"])):
        if digest(target / name) != expected:
            raise ValueError(f"{backend}: staged checksum mismatch for {name}")
    loader_dest = target / "vulkan-1.dll"
    if loader and loader.is_file():
        shutil.copy2(loader, loader_dest)
    elif loader_dest.exists():
        loader_dest.unlink()  # use the OS Vulkan loader instead of a stale copy
    # Publish manifest only after the pair is complete and verified.
    (target / "renderer-pair.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--d3d12-sdk", required=True, type=Path)
    ap.add_argument("--vulkan-sdk", required=True, type=Path)
    ap.add_argument("--destination", required=True, type=Path)
    ap.add_argument("--vulkan-loader", type=Path)
    args = ap.parse_args()
    try:
        d3d12, runtime, plugin = checked_source(args.d3d12_sdk, "d3d12")
        stage(args.destination, "d3d12", d3d12, runtime, plugin)
        print("Staged verified D3D12 renderer pair")
        vulkan_manifest = args.vulkan_sdk / "renderer-sdk-provenance.json"
        if vulkan_manifest.is_file():
            vulkan, vruntime, vplugin = checked_source(args.vulkan_sdk, "vulkan")
            for key in ("sdk_commit", "patch_sha256", "worktree_diff_sha256",
                        "compiler", "compiler_version"):
                if vulkan[key] != d3d12[key]:
                    raise ValueError(f"Vulkan and D3D12 differ in {key}; rebuild "
                                     "Vulkan with tools/build_sdk_vulkan.cmd")
            stage(args.destination, "vulkan", vulkan, vruntime, vplugin,
                  args.vulkan_loader)
            print("Staged verified Vulkan renderer pair")
        else:
            stale = args.destination / "renderers" / "vulkan"
            if stale.exists():
                shutil.rmtree(stale)
            print("Vulkan SDK snapshot not present; Vulkan launcher disabled. "
                  "Run tools/build_sdk_vulkan.cmd then rebuild Release.")
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"Renderer staging failed: {exc}") from exc


if __name__ == "__main__":
    main()
