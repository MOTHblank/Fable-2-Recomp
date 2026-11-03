# Upstream open pull requests — review (2026-11-02)

Reviewed open PRs against `himdo/Fable-2-Recomp` after the 45-commit
`master` audit. There were four open PRs at review time. This is a
source-level review; a Windows build and gameplay run were not performed.

| Upstream PR | Status for this fork | Reason and next action |
|---|---|---|
| [#51 — F6 graphics and GPU backends](https://github.com/himdo/Fable-2-Recomp/pull/51) | **Graphics source integrated** | Its SDK PR [#4](https://github.com/himdo/rexglue-sdk/pull/4) was rebased selectively onto newer MOTHblank SDK: effects, shader arrays, material replacement, F6 dialog and cvar save recovery. Keep the fork's separate D3D12/Vulkan runtime snapshots and launcher. D3D12 shader assets staged and included in releases. New FX are opt-in; Windows/gameplay validation remains outstanding. |
| [#71 — Remove legacy hero/dog texture toggles](https://github.com/himdo/Fable-2-Recomp/pull/71) | **Deferred** | The fork still supports optional `realtime_texture_morphing` and fallback readback settings, and its codepaths are configured separately. Forcing always-on morphology before validating both supported game revisions risks a visual regression. Review after the graphics integration is verified. |
| [#80 — Linux/Vulkan build](https://github.com/himdo/Fable-2-Recomp/pull/80) | **Deferred** | Valuable cross-platform work, but its in-tree SDK patches and POSIX diagnostics need adaptation to the fork's newer SDK, Windows-first WPF launcher, and separate Vulkan runtime snapshots. Avoid merging the older SDK patch or replacing the current build layout. |
| [#123 — Upstream release update button](https://github.com/himdo/Fable-2-Recomp/pull/123) | **Not copied** | PR unconditionally queries upstream GitHub releases on launch, exposing a user's IP and adding network work, with no opt-out. The MOTHblank fork has a distinct release channel. Any future implementation should be opt-in, target the fork's release feed, use bounded timeouts, and gracefully handle offline use. |

## SDK PR relationships

- [himdo/rexglue-sdk #3](https://github.com/himdo/rexglue-sdk/pull/3)
  (Vulkan image-acquire present-gate fix) was already merged upstream and
  retained in the newer SDK fork.
- [himdo/rexglue-sdk #4](https://github.com/himdo/rexglue-sdk/pull/4)
  (effects/material shaders/F6/config-save fix) was still open. The
  MOTHblank SDK fork now carries its source adapted over upstream's newer
  GPU and input/runtime work, plus its original Fable-specific patches.
- Shader development and feature controls: [graphics enhancements](GRAPHICS_ENHANCEMENTS.md).
  Remaining in-game verification: [graphics port status](GRAPHICS_PORT_STATUS.md).

No upstream PRs were merged using GitHub's merge button; compatible code
was adapted directly to `MOTHblank/Fable-2-Recomp:master` and
`MOTHblank/rexglue-sdk:main`. No GitHub Actions were run.
