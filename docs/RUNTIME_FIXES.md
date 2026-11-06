# Windows runtime fixes

The active native SDK source is the tracked
[MOTHblank/rexglue-sdk](https://github.com/MOTHblank/rexglue-sdk) submodule
under `thirdparty/rexglue-sdk`. Its pinned revision combines newer upstream
runtime and renderer improvements with Fable-specific D3D12 pipeline,
frame pacing, mouse and adaptive-prompt changes.

The historical base for these fixes was `himdo/rexglue-sdk` commit
`1338ec1011739c7f00f8df9b9473e34d3dd9f2df`. The
`thirdparty/rexglue-sdk-runtime-fixes.patch` and
`thirdparty/rexglue-sdk-debug-exports.patch` document the older audio,
frame-pacing and Windows-export changes. Those changes are already integrated
in the current SDK fork; preparation verifies their source markers instead
of applying old patches again. Five Fable-specific follow-up patch sets have
also been incorporated into the SDK fork, and are verified rather than
reapplied at its pinned revision.

The pinned SDK fork now builds the ReXGlue codegen executable itself. The
Windows workflow installs `rexglue.exe`, runtime/plugin binaries, headers and
CMake package from one source revision; no prebuilt SDK is mixed into the
native package. Game disc images and saves are not part of the SDK.

## Build

In a Visual Studio Windows x64 developer shell with LLVM 20+, Ninja,
CMake, Python (plus `pefile`), Git, and .NET 8 SDK on PATH:

```cmd
build.cmd
```

The default Release workflow builds and stages the launcher in `out/game`,
extracts a user-supplied retail ISO there if needed, builds and installs the
matched source SDK (including `rexglue.exe`), generates guest code with that
matched tool, then builds the game and stages the renderer pair. The
launcher and game executable are both in `out/game`. The Release preset
enables post-build PE cleaning; profiling can opt out.

For distributable artifacts:

```cmd
build.cmd package
```

Only allowlisted binaries, renderer pairs, project Lua scripts and canonical
config are archived in `out/dist`. Retail ISO contents, `default.xex`,
`dir.manifest`, saves, caches, logs and user configuration are not shipped.

`tools/setup_sdk_src.cmd` synchronizes the SDK fork remote and checks out
the tracked submodule revision. `tools/prepare_runtime_sdk.py` refuses
partial SDK modifications or missing integrated fixes; it does not overwrite
local edits. A Debug SDK may also be built separately using
`tools/build_runtime_sdk.cmd` with its Debug argument. The Vulkan SDK uses
the same source through `tools/build_sdk_vulkan.cmd` and retains a separate
runtime snapshot. If its provenance is incompatible with D3D12, the
renderer staging process rejects the mismatched pair.

## Behavior

When SDL cannot initialize or open an audio device, each affected audio client
gets a silent, clocked consumer at 256 samples / 48 kHz. It returns frame
permits at approximately 5.33 ms intervals, performs no guest-memory reads,
does not produce idle permits or catch-up bursts, and joins before shutdown.
Working SDL devices continue using the existing audio path. This is a startup
fallback, not hot-plug recovery or a general sound-quality fix.
`StartWithoutAudio.cmd` forces an invalid SDL3 backend in the launched process
only. It does not change Windows device settings. Start a fresh game/launcher
process for this test, then load a save and exercise gameplay. Relaunch
normally to test real sound output.

`frame_limit` is a host swap limiter: 0 disables it, with SDK range 0–240.
The launcher offers 30, 60, 120, 144, 165, 240 and unlimited. Fable waits for two guest vblanks;
the old 60-Hz guest pacing therefore held it at 30 even with a 60-FPS limit.
`guest_vblank_pacing` keeps the old SDK behavior by default, but the launcher
disables it for 60/unlimited independently of presentation VSync. The limiter
uses a private high-resolution waitable timer on Windows, with a standard wait
fallback, and does not change the guest clock or advertised video mode.
F3 counts guest swaps in Release, averaged over a short window and including
stalls; it does not count monitor refreshes or establish unique rendered frames.

Hero/dog resolve readback is the upstream fix, not a new implementation here.
Credit for the original approach/address goes to just-harry's Unofficial Xenia
femtofork, with the ReXGlue implementation maintained upstream. Explicit
readback configuration still takes precedence over the application's default.

The SDK's D3D12 renderer dropped draws while their pipeline was still being
created in the background (`async_shader_compilation`, on by default). A
dropped draw leaves its object missing for that frame, and when the dropped
pass renders into a texture, everything sampling that texture shows stale or
uninitialized data: noise bands, colored streaks and short bursts of
corruption, mostly when new effects or areas first appear. The fix is made in
rexglue-sdk itself (himdo/rexglue-sdk, not a patch here): draws wait for their
pipeline instead, with the waiting thread helping to create queued pipelines.
Async compilation stays on; only the first use of a pipeline can stall.
`async_pipeline_wait = false` in the SDK config restores the old skipping, and
`async_pipeline_wait_timeout_ms` (default 5000) bounds a single wait.

The dog's fur flickers because of depth ties. The fix is made in rexglue-sdk
itself (himdo/rexglue-sdk, not a patch here): the SDK cvars
`depth_bias_shader` (a guest pixel shader hash) and `depth_bias_shader_units`
add an extra host depth bias for draws that use that shader. Fable II draws the
dog's fur as 15 thin shell layers over the body with pixel shader
`014F8A02DB7B19CA`, a greater-or-equal depth test and no polygon offset. The
layers sit about 0.001 units apart and rely on exact ties in the Xbox 360's
24-bit floating-point depth buffer. The host stores 32-bit depth, so the same
layers land a hair in front or behind depending on rounding, and about half of
them fail: flickering, blocky patches on the dog that get worse at higher
internal resolutions. A bias of 16 units (about two 24-bit depth steps) turns
the near-ties back into passes; in testing it removed the dog's flickering at
1x, 1440p and 4K, and 64 looked the same as 16. `[patches] dog_fur_depth_bias` in
`fable2_config.toml` (default `true`) seeds the cvars in the app; an explicit
`depth_bias_shader` in the SDK config takes precedence. The SDK's
general exact-24-bit mode (`depth_float24_convert_in_pixel_shader`) also
addresses this but breaks other rendering in this game. The bias applies to
the host render-target path only, not ROV.

## Validation and remaining limits

Both unchanged USA/EU and German GOTY images started with the same native EXE.
On the German GOTY build the tester loaded a Version 1 adult-hero save and
confirmed normal hero and dog rendering. The tester also confirmed up to
60 FPS with the cap and approximately 150 FPS in unlimited mode. Launcher
tests cover edition validation, output/render mappings, FPS mappings,
preferences surviving runtime rewrites, unknown-setting preservation and
dropdown contrast. Synthetic audio, limiter and FPS-meter tests passed, and
the patch applied cleanly and idempotently to a fresh SDK checkout.
Both build paths were subsequently checked with a fresh pinned source SDK
and full `build.cmd fable_2` / `build.cmd -release fable_2` builds.
The Debug EXE and plugin were checked to import `rexruntimed.dll`; staged DLL
code sections match the built SDK after the existing PE metadata normalization.
Staging tests check both configurations
and fail before copying if Debug inputs are missing. The SDK follow-up patch
was also checked on an SDK already carrying the original patch, and reapplies
idempotently. A deliberately mismatched host/SDK configuration was rejected.

The forced missing-audio-backend build started successfully for the tester;
SDL failure-path timing and shutdown were also checked separately. This does
not establish every physical-device failure, audible playback on all devices,
or unplug/replug recovery. There is no claim that long-session simulation,
cutscenes or timing above 60 FPS are fully correct. Frame drops below 20 FPS
were reported and remain unresolved. Other regional/TU revisions, exhaustive
save compatibility and remaster features are outside this change.

## Renderer cache warmup and Vulkan diagnostics

D3D12 already persists guest shaders and pipeline descriptions under the
runtime's `cache/shaders/shareable/` directory when the SDK's `store_shaders`
setting is enabled (the default). Existing stored pipelines are translated and
created when the title's shader storage is initialized, before normal gameplay.
Do not delete the `cache/` directory between launches if you want to reuse
that work. First-ever effects cannot be precompiled until the game has seen
and recorded their shader and pipeline combinations.

`thirdparty/rexglue-sdk-pipeline-warmup.patch` corrects two SDK defects:
its warmup loop now starts the extra compilation workers it calculates, and its
storage writer drains pending shader/pipeline records during shutdown instead
of silently discarding them. `tools/prepare_runtime_sdk.py` applies this patch
to both the clean SDK pin and accepted fork revisions carrying the original
runtime fixes. The existing `async_pipeline_wait` correctness behavior is
unchanged: a required pipeline is never intentionally skipped merely to hide
first-use compilation latency. Warmup cost may move to the loading phase, and
no claim is made that an empty cache eliminates all first-use stalls.

The optional, older Vulkan source patch
(`thirdparty/rexglue-sdk-local.patch`) retains its gamma-ramp and renderer
correctness fixes, but guards the temporary GPU readback functions, periodic
swap and gamma-ramp readbacks, background mapping monitor and verbose telemetry
behind `FABLE2_VULKAN_DIAGNOSTICS`. The wrapper
(`tools/vulkan_sdk/CMakeLists.txt`) defaults this flag to OFF and defines it
only for Debug builds when explicitly enabled with
`-DFABLE2_VULKAN_DIAGNOSTICS=ON`. Release builds exclude these diagnostics
even if the CMake option is ON. Lightweight frame-time/FPS counters and
meaningful error logs remain available without the flag.

**Deployment note:** the normal Windows build uses the pinned source SDK and
its separate runtime patches; editing the older optional Vulkan patch does not
change already-built DLLs. Reconfigure and rebuild the corresponding SDK to
pick up the changes. Any older Vulkan SDK checkout with the unguarded version
of the patch must be recreated/repatched before being used for production.
