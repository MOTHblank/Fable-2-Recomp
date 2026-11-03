# Graphics PR #51 — source integration status

Upstream game PR: https://github.com/himdo/Fable-2-Recomp/pull/51
Upstream SDK graphics PR: https://github.com/himdo/rexglue-sdk/pull/4
SDK Vulkan present fix: https://github.com/himdo/rexglue-sdk/pull/3

## Integrated into the MOTHblank repositories

- **SDK graphics source:** Ported SDK PR #4's F6 effects overlay, scene-effect
  render-target integration, D3D12 and Vulkan effect implementations, original
  shader sources and precompiled shader arrays, and D3D12 material-shader
  replacement engine into `MOTHblank/rexglue-sdk`.
- **Newest SDK changes preserved:** Overlapping SDK files were reconciled with
  the current upstream base plus our pipeline warmup, mouse, present-pacing and
  adaptive-prompt changes. The Vulkan pre-acquire present-gate fix from SDK PR
  #3 was already upstream and retained.
- **Configuration saving:** Recovered individual parseable TOML settings on
  malformed files, kept unknown/default-valued existing keys, and preserved
  on-disk values over command-line or environment launch overrides.
- **F6 input:** The SDK input-activity callback now deactivates game MnK input
  while the F6 overlay is open, allowing the event-driven mouse driver to
  release pointer capture for UI interaction.
- **Default behavior:** The new effect passes and replacement material
  shaders are off by default. Players may opt in using F6 presets or cvars.
- **Game-specific shaders:** Original HLSL, includes, compiled DXBC variants,
  shader-building tools, and shader-analysis tools are tracked and staged.
  `build.cmd package` includes the compiled material files.
- **Rendering selection:** The fork continues to use the WPF launcher's
  renderer selector and *separate*, provenance-checked D3D12 and Vulkan SDK
  snapshots. Private renderer sessions share the staged materials through a
  junction. The original PR's `gpu_backend` cvar, unified dual-backend DLL,
  and SDK build-command replacement are intentionally not imported.
- **SDK pin:** The game submodule references
  `MOTHblank/rexglue-sdk`; `tools/prepare_runtime_sdk.py` explicitly
  verifies the integrated F6, effects, materials and save components and
  avoids reapplying the historic patches.

See [Graphics enhancements](GRAPHICS_ENHANCEMENTS.md) for effects, controls,
shader sources, and known limitations.

## Remaining work: Windows build and real rendering verification

This port has been committed and statically checked at the source level, but
the combined code has **not** been compiled or played on Windows. No assertion
is made that the F6 overlay, saved settings, shaders, or renderer paths are
runtime-correct yet. Verification should cover:

1. A clean D3D12 source SDK build and game link with the integrated
   `EffectsDialog` and material shader loader.
2. First launch with defaults off, followed by enabling individual effects
   and the High preset in F6, saving, restarting and confirming persistence.
3. D3D12 material replacements and scene-effect render order (before game
   bloom/tonemapping, UI remains clear).
4. Open/close F6 while mouselook is active: cursor release, no delayed mouse
   movement, game inputs restored on close.
5. A separately rebuilt Vulkan runtime/plugin pair with matching SDK source
   provenance, including the present-gate fix; remember the upstream PR
   documented missing ground rendering and weaker shadow support on Vulkan.
6. ZIP packaging: both runtime pairs where available, and exactly the
   authored material DXBC variants with no retail content.

## Other PR #51 components intentionally not copied wholesale

- `src/core/gpu_backend.h` and the corresponding `fable2_config.toml`
  graphics/backend section (fork's launcher already selects isolated
  renderer pairs through `launch_renderer.ps1`).
- Replacement `tools/build_runtime_sdk.cmd`, deleted
  `tools/build_sdk_vulkan.cmd`, and rewritten renderer CMake glue (would
  discard independently checked runtime provenance).
- The PR's historical keyboard gamepad F6 capture code: the fork uses the
  event-driven SDK MnK driver and gates capture at the SDK overlay instead.
- Legacy PR build/tests commands and game output paths. The fork's canonical
  install and extraction root remains `out/game`.

Source integration is complete for the F6 renderer and material codepaths;
successful gameplay validation remains outstanding.
