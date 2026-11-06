# Upstream reconciliation — 2026-11-06

This records the source-level reconciliation of `MOTHblank/Fable-2-Recomp`
with `himdo/Fable-2-Recomp:master` and of `MOTHblank/rexglue-sdk` with
`himdo/rexglue-sdk:main`.

After the reconciliation merge, both forks report **0 commits behind** their
respective upstream branches. The merge preserves the MOTHblank trees rather
than replacing fork-specific launcher, input, renderer or packaging work.

## Newly adapted upstream game work

- PR #127, developer console and fit-to-window support, was ported as a unit:
  console host/file transport, Lua command pump, GUI-Lua bridge, widescreen
  hooks and analysis tools. It was adapted to the fork's SDK-owned
  `MnkInputDriver` rather than restoring the older polling keyboard driver.
- The console's synthetic pad and the Debug remote-control pad now have
  distinct device IDs, avoiding a collision that the direct upstream port
  would have introduced in this fork.
- PR #136's native main-menu Quit item was adapted without requiring the large
  upstream manifest symbol-renaming change.
- PR #129 and PR #137 are test-only changes and do not alter the fork runtime.

## SDK reconciliation

The SDK fork incorporates all current upstream-main commits and records upstream
history as a merge parent. It additionally keeps the Fable-specific input,
renderer, graphics-effects and runtime work.

The SDK now exports the symbols needed by its own `rexglue.exe`, supports
opting out of CMake package-registry writes, and uses current ImGui modifier
events.

The default Windows build no longer mixes an official prebuilt codegen package
with source-built native libraries. `build_runtime_sdk.cmd` builds and installs
one complete package from the pinned SDK revision: headers, CMake package,
`rexruntime.dll`, `rexgpu-xenos.dll` and `rexglue.exe`. Provenance records
the codegen SHA-256, and the game CMake rejects a codegen executable whose hash
does not match the staged SDK metadata.

## Deliberate exclusions / retained fork behavior

- Upstream PR #123's launcher update checker is not adopted. The fork retains
  its no-unsolicited-update-query behavior and separate release channel.
- Open PR #71 is not adopted. Its own upstream discussion notes that forcing
  realtime texture morphing breaks the current Vulkan path while the targeted
  readback fallback works. The fork therefore retains both compatibility
  controls.
- Open PR #80 (Linux/Vulkan) is not part of upstream master and is not included
  in this master reconciliation. The fork retains its existing Windows Vulkan
  snapshot workflow rather than applying PR #80's older in-tree SDK patch.
- GitHub Actions changes remain excluded from the fork workflow.

## Reconciliation commits

- Fable upstream-history merge: `63c622abdb964ba958fd6db3d5239231b5803582`
- SDK upstream-history merge: `bfca57d54eb041baeaba95f79a9d2dc68feb7dfb`

This document describes source integration, not a Windows runtime
certification. A local Windows build and gameplay run are still the authority
for binary/runtime behavior.
