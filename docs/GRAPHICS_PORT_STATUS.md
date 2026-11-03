# Graphics PR #51 — integration status

Game PR: https://github.com/himdo/Fable-2-Recomp/pull/51
SDK PR: https://github.com/himdo/rexglue-sdk/pull/4

## Integrated on MOTHblank/master

- WPF launcher renderer selector (D3D12 by default, Vulkan experimental).
  Uses verified renderer pair snapshots and the existing isolated
  `launch_renderer.ps1`, not the proposed unified SDK DLL path.
- Renderer choice persisted as `launcher_renderer` solely in
  `launcher-settings.toml`, avoiding unsupported `gpu_backend` cvars
  in the current SDK's `fable_2.toml`.
- Original HLSL material shader sources and includes, three compiled DXBC
  variants, the `build_material_shaders.py` compiler utility, and
  `xenos_to_hlsl.py` shader study tool.
- CMake stages `materials/d3d12` into the game's install directory.
  Isolated Vulkan renderer sessions can see the same material directory.
  Neither the SDK submodule nor runtime/plugin hashes were changed.

## Still required: SDK graphics forward-port

**F6 effects and material replacement are NOT available with the currently
pinned SDK.** The compiled shaders will be staged but not loaded.

The current fork pins `himdo/rexglue-sdk` at
`92ddff153f98b9bdd385bbf72cf0dca9a7cda4b3`.
Upstream SDK graphics PR #4 targets an older tree ending at
`9b4ec5c3ac4d7ec40af81b1fa34a1a7fc578c624`. The pin contains 29
commits not present on that graphics branch; switching the submodule
pointer directly would discard important SDK changes.

To complete this port:

1. Forward-port SDK PR #4's render-target processing, postprocessing
   effects, material replacement hooks, overlay, and cvar save fixes onto
   the newer SDK, resolving overlapping changes explicitly.
2. Preserve input, native mouse/camera integration, runtime patches,
   function tracing and per-backend SDK snapshot provenance. Update
   `tools/prepare_runtime_sdk.py` validation for the new verified SDK pin.
3. Build matching SDK headers/runtime/plugin and the native game for Windows.
   Keep enhancement effects disabled by default until the result is inspected.
4. Only then expose F6, replacement-material runtime controls and the
   graphics settings. Do not expose controls that are inert.
5. Vulkan is separately experimental. The original PR documents missing
   ground rendering and broken effect shadows.

Do not wholesale merge PR #51's `.gitmodules`, renderer build scripts,
`fable2.cmd` or CMake replacement: that would conflict with the fork's
verified, independent D3D12/Vulkan runtime pair system.
