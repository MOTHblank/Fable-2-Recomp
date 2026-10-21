# Fable 2 — ReXGlue Recompiled Project
First and for most this project was made to test the capabilities of local and open source AI. This project was made using Qwen3.8 27b. After about an hour it got the game running. Then after a large amount of human trial and error I was able to get enough of the functions mapped to be able to complete the game. I currently do not consider this to be fully complete as I have yet to 100% the game. The sha256 of the iso that started this project is: "685a0d3bea9718812f17bcd155907a5359a548b6d3d8342dd2a6c944f45e35ff" and its the Fable 2 GOTY for USA and Europe.

Recompilation of Fable 2 (Xbox 360, title ID 4D5307F1) using the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) v0.10.0. Guest PPC code is statically recompiled to C++ at build time by `rexglue codegen`, driven by `fable_2_manifest.toml`.

# Important Note:
**This project does not condone Piracy, any mentions or links will result in removal and an instant ban from this repo!**

# Current and planned features

Original GOTY USA/Europe and German GOTY dumps are supported by the default
`goty-compatible` build profile. Other XEX revisions remain rejected. See
[German GOTY validation and known limits](docs/GERMAN_GOTY_SUPPORT.md).
[x] Can be used to beat the game\
[x] Guild chest fully unlocked\
[x] Uncapped framerate / increased framerate\
[ ] Higher Resolution support\
[x] Built in Debug Menu
  - [x] Enabling custom lua to run in game

[x] Keyboard / Mouse Support\
[ ] In game Text changed to respect keyboard and mouse
  - [ ] Automatic swapping between text

[ ] Increased performance / framerate\
[x] Hero / Dog Texture bug fix (upstream renderer, verified on German GOTY)\
[ ] Vulkan support\
[ ] Linux Builds\
[x] Custom commands to aid in debugging\
[ ] Improved Graphics rendering\
[ ] Custom menu(s) / modifying menus for extra functionality (like closing the game)\
[x] Supporting other languages
  - [x] Germany
  - [x] France
  - [x] Italy
  - [x] Russia
  - [x] Spain

[x] Custom Launcher
  - [x] Built in ISO extractor

## Windows: one build, one game folder

On Windows, install the prerequisites listed under **Building** below, then run
`build.cmd` from the repository root. This builds the Release native game,
its matched D3D12 runtime, and the self-contained WPF launcher. Start
`out\game\\Fable2Launcher.exe`, click **Extract from ISO** and select your
own Fable II disc image. It extracts `default.xex` and `data/` into
`out\game`; launch the game with the launcher. The game executable is
`out\game\\fable_2.exe` (not `fable.exe`).

Intermediate compilation artifacts stay in `out\build\\win-amd64-release`.
Do not point the launcher at that folder. Rebuilds keep extracted content and
saved settings in `out\game`. The launcher is built automatically by `build.cmd`. No wrapper scripts,
extra commands or manual file copies.

## Windows launcher

The optional WPF launcher detects the original game edition and configures
output resolution, internal render scale, anisotropic filtering, FXAA, VSync,
window/fullscreen mode and a 30/60/120/144/165/240/unlimited FPS limit. It keeps its preferences
in `launcher-settings.toml` and writes `fable_2.toml`, preserving other settings
and a one-time backup. Preferences survive the runtime rewriting its config.
It can also extract the game straight from a disc image: the **Extract from
ISO** button opens a raw `.iso`, pulls out just `default.xex` first, checks its
SHA-256 against the supported-version list, and only once it matches does it
extract the rest of the disc (`default.xex` and `data/`) into the launcher's own
folder, with a live progress screen and a Stop button. It reads Xbox 360 GDFX
discs with the shared [X360Extract](tools/x360extract/) library, so its output
matches the `x360extract` tool byte for byte — making the launcher the simplest
way to get the game files in place, with no separate extractor tool.

The game folder is always `out\game` in a source checkout. The launcher and
`fable_2.exe` are built together in this folder; **Extract from ISO** puts
`default.xex` and `data/` in the same location. There is no separate path
selection, moving of game files or per-configuration extraction. Both
applications are named Fable 2 Recompiled and use project-owned icons.





# Notes for running the game
- **How to extract (built-in, recommended):** the launcher has an **Extract from ISO** button. Click **Extract from ISO** and pick your disc image (`.iso`). It first checks the iso against the supported-version list, and only once that matches does it extract disc (`default.xex` and `data/`) into the launcher's own folder. No separate tool is required.
- **You must supply your own game disc ISO** (not in this repository). `build.cmd` starts the launcher to extract it into `out\game` when missing.   
- **Saves live in `out\game\\saves\`** — they are preserved across rebuilds.
- The `--game_data_root <path>` override still points the content root at a different tree (e.g. to run from a shared content copy without staging); saves/cache still land next to the exe.
- Although this was made for the US / Europe GOTY version I have been able to run the following regions: Germany, France, Italy, Russia, and Spain. Your milage may very if you use those regions.


## Running

Run `build.cmd`, complete the ISO extraction prompt if necessary, then start
`out\game\\Fable2Launcher.exe` and click **Launch**. All game content and
both executables live in `out\game`.


Optional command-line overrides (all normal `--cvar value` args):

| Argument | Effect |
|---|---|
| `--game_data_root <path>` | Use a different content directory (beats the exe-dir-only search) |
| `--update_data_root <path>` | Optional update content root |
| `--window_width` / `--window_height` / `--fullscreen` | Presentation options |
| `--keyboard_gamepad_map <map>` | Host keyboard -> guest gamepad button map (see below) |
| `--skipHash` | Skip the `default.xex` SHA-256 integrity check. The hash is still written to the log, but a mismatch no longer blocks launch (useful for a modified or non-catalogue XEX) |

## User config (fable2_config.toml)

The recomp has its own human-readable user config, `fable2_config.toml`, next
to the exe — separate from `fable_2.toml`, which is the ReXGlue SDK's cvar
config. It is staged by the build (once; user edits survive rebuilds) and the
game recreates it with defaults on launch if it is ever missing. Loaded in
`Fable2App::OnPostInitLogging()`; code in `src/core/fable2_config.{h,cpp}`.

- Missing keys -> built-in defaults; wrong types / unknown sections -> logged
  warnings, defaults used; a syntax error -> dialog + defaults (never blocks
  launch).
- To add a setting: add a member to `fable2::config::Values` (with default),
  read it in `Load()`, and document the key in `config/fable2_config.toml`
  **and** the embedded template in `src/core/fable2_config.cpp` (keep both in
  sync).
- Input settings are seeded into the SDK's native keyboard/mouse driver
  at startup. Higher-priority `fable_2.toml`, `REX_*` environment and CLI
  overrides are respected. Changing the legacy map or sensitivity cvars in
  F3 also updates the active SDK bindings.
- The `[patches]` section holds runtime toggles for the recomp-level
  (mid-asm hook) patches, consulted by the hook bodies on every call
  (`src/core/fable2_hooks.cpp`) — flip one and relaunch to A/B a patch with no
  rebuild. This includes `high_tick_rate` and `higher_hf_tick_rate` (Xenia
  "High Tick Rate" / "Higher HF Tick Rate"; turn both on together), which
  also drive the guest-image data writes below. There is no separate patch
  file.



## Adaptive button prompts (experimental)

The recomp can replace supported inline Xbox button markup in **writable game
UI strings** with labels taken from the current keyboard/mouse bindings.
`--ui_prompt_mode auto` (the default) follows the most recently active
keyboard/mouse or physical controller; `keyboard` forces key labels, and
`controller` always preserves the original Xbox prompts. The matching
runtime SDK is required for automatic switching.

This is an early, reversible text-markup substitution, **not yet universal**:
prompts rendered from prebuilt graphics, cached layouts, or different markup
may remain Xbox-styled. See [Button prompt implementation and
limitations](docs/BUTTON_PROMPTS.md).

## Keyboard controls

The game normally reads a controller through the Xbox 360 `XamInputGetState`
API. The SDK's existing `MnkInputDriver` now handles keyboard events, mouse
buttons and relative mouse movement. It is registered by ReXGlue and enabled
from the game's `fable2_config.toml`. Physical gamepads continue to work.
The old `KeyboardGamepadDriver` has been removed; its binding parser is
retained for launcher compatibility.

The mapping is the `keyboard_gamepad_map` cvar, format `Key:Button,Key:Button,...`. Its default is no longer hardcoded in the binary: it comes from `[input] keyboard_gamepad_map` in `fable2_config.toml` (see User config above), which you can edit to remap permanently. The command line and the F3 console still override it per-launch / live.

- **Key** — a host key name understood by `rex::ui::ParseVirtualKey`
  (`E`, `Space`, `LeftShift`, `F1`, ...), or a mouse button: `LMB` (left),
  `RMB` (right), `MMB` (middle), and `XMB1`/`XMB2` (side buttons). Mouse
  button names are case-insensitive; a held mouse button behaves exactly like
  a held key.
- **Button** — a guest gamepad input: `A`, `B`, `X`, `Y`, `LB`/`RB` (shoulders),
  `LT`/`RT` (triggers), `Up`/`Down`/`Left`/`Right` (dpad), `Pause` (Start),
  `Select` (Back), `L3`/`R3` (thumb clicks), and `StickUp`/`StickDown`/
  `StickLeft`/`StickRight` (left thumbstick, full deflection while held).

The default layout (mouse buttons plus keyboard) is:

| Key(s) | Guest input |
|---|---|
| `LMB` | `X` |
| `RMB` | `Y` |
| `MMB` / `B` | `B` |
| `Shift` / `E` | `A` |
| `W` `A` `S` `D` | Left stick (up / left / down / right) |
| `Q` / `R` | Left / Right trigger |
| `Z` / `Tab` | Left / Right shoulder |
| Arrow keys | Dpad up / down / left / right |
| `Escape` | Pause (Start) |
| `M` | Select (Back) |


Remap at launch without recompiling, e.g.

```
fable_2.exe --keyboard_gamepad_map "E:A,B:B,Space:L3,Enter:Start"
```

### Mouse look (right stick)

Mouse movement now comes from **SDL3 relative mouse mode**, with cursor
recentering used only as a fallback. Fractional mouse motion is accumulated
until the guest input poll. The mouse drives the game's native right-stick
camera path, preserving its camera limits. The following cvars control it:

| Argument | Effect |
|---|---|
| `--mouse_look <bool>` | Enable/disable mouse look (default `true`) |
| `--mouse_look_scale <n>` | Horizontal sensitivity (default `256`) |
| `--mouse_look_scale_y <n>` | Vertical sensitivity (default `256`) |
| `--mouse_invert_y <bool>` | Invert vertical movement (default `false`) |
| `--mouse_deadzone_compensation <n>` | Optional mouse-only deadzone compensation, 0..12000 (default `0`) |

The mouse always uses the game's native right-stick camera handling.
The retired `direct_mouse_look` and `mouse_look_legacy_orbit` modes and
related orbit tuning keys are not supported; remove them from older config
files. Use the settings above instead. Camera telemetry
(`mouse_look_camera_diag` / `mouse_look_camera_dump`) remains available.

The defaults come from `[input] mouse_look` / `[input] mouse_look_scale` in
`fable2_config.toml` (edit there to change them permanently), the command
line overrides per launch, and both cvars are hot-reloadable from the in-game
console, so you can dial in the sensitivity live. Example: `fable_2.exe
--mouse_look_scale 512` for a more sensitive camera.

The launcher edits `fable2_config.toml`, with individual SDK `keybind_*`
overrides supported in `fable_2.toml`. F3 supports live remapping and
sensitivity updates. The F4 debug-menu key is reserved from the gamepad map;
F5 still executes its external Lua file independently of the input driver.
SDK focus and UI activation govern cursor capture.

## F5 — run an external Lua script

Pressing **F5** (host keyboard) runs an external Lua file in the in-game Lua
state, exactly the way the game's own `RunScript(path)` global does — but
triggered from the host. This lets you drop a plain `.lua` file on disk and run
it against the live game (no recompile of the scripts needed).

- **Default file:** `data/scripts/recomp/F5.lua` (the build stages
  `src/lua/*.lua` into `data/scripts/recomp/` next to the exe). The shipped
  `F5.lua` snapshots the hero's position (`QuestManager.HeroEntity:GetPosition()`)
  and shows `X / Y / Z` in a message box.
- **Path:** set by the `f5_lua_path` cvar (default `scripts/recomp/F5.lua`,
  resolved relative to the VFS root `data/`). Override per-launch with
  `fable_2.exe --f5_lua_path "scripts/other/MyScript.lua"`.
- **How it works:** `src/core/fable2_f5_lua.h` captures the
  `CScriptManager::RunScript` callable the first time the game loads a `.lua`
  script (via a probe on the LuaPlus bound-method dispatcher), then replays that
  call with your path when F5 is pressed. The file is loaded fresh on each press,
  so you can edit it live (the VFS re-reads it).
- **The script runs in the game's global Lua environment**, so it has the full
  game API (`QuestManager`, `Debug`, `GUI`, `Creature`, `Player`, ...). Plain
  text is fine — `RunScript`/`loadfile` compile it for you.

Implementation: F5 edge-detection in `src/input/keyboard_gamepad.h`, a per-frame
replay from the `MainRenderLoop` hook in `src/diagnostics/fps_meter.h`, and the
string-build + `RunScript` call in `src/core/fable2_f5_lua.h`.\
<br>
<br>
<br>
<br>

---


# Notes for dev who want to work on the build:

## Guest-image patches

Data patches for the loaded `default.xex` guest image (Xenia game-patches
ops), applied before the guest module launches: `Fable2App::OnPostLoadXexImage()`
→ `fable2::patches::ApplyAll()` (code in `src/core/fable2_patches.{h,cpp}`).
There is no patch file: each patch is switched on by a `[patches]` key in
`fable2_config.toml` (currently `high_tick_rate` and `higher_hf_tick_rate`).
**Scope: data patches only** — code-region ops are inert in this recomp
(guest `.text` is never executed); code patches are mid-asm hooks instead.
Full details: `docs/patches.md`.

## Guest function-call tracing (fable2_func_trace.log)

To figure out what each recompiled function does, every guest function entry
can be logged by name. Codegen emits `REX_FUNC_PROLOGUE()` at the top of
every function in `generated/default/fable_2_recomp.*.cpp`; the build hooks
that one macro (via `src/core/fable2_func_trace.h`, appended to the recompiled
PCH after the generated pch — no generated files are modified) so each entry
logs its name to `fable2_func_trace.log` next to the exe. Consecutive calls
of the same function are run-length encoded (per thread) to keep the file
small:

```
GetNewGameLoadingGlobal
sub_82189708 x 4821
LoadingScreen_Virtual43
sub_82CC1BC0 x 3
...
```

(`x N` means that function was called N times in a row; a bare name is a run
of one.)

**Session summary:** call counts are accumulated separately and written to
`fable2_func_summary.log` (same folder), one line per function, sorted by
total count:

```
18422331 x MainRenderLoop_82B9CD68
9711204 x __restgprlr_28
54 x Story_FirstChildCombat
```

It's refreshed every 5 s while tracing (and once more on clean exit), so you
can watch it in another window to see at a glance what's being called; the
last write is at most ~5 s stale even if the game exits via ExitProcess.
Counts respect the same on/off + filter as the text log.

Off by default (one atomic load per call when off). Enable with:

```
set FABLE2_FUNC_TRACE=1            rem every guest function entry
set FABLE2_FUNC_TRACE_FILTER=LoadingScreen   rem optional: only names containing this
```

Either output can be turned off independently (default: both on):
`FABLE2_FUNC_TRACE_LOG=0` skips `fable2_func_trace.log` (summary only),
`FABLE2_FUNC_TRACE_SUMMARY=0` skips `fable2_func_summary.log` (trace only).

or at runtime from a named-function override (see `src/diagnostics/fps_probe.h` for the
override pattern): `Fable2FuncTraceSetEnabled(true)` /
`Fable2FuncTraceSetFilter("LoadingScreen")` /
`Fable2FuncTraceSetSubsOnly(true)` /
`Fable2FuncTraceSetLogEnabled(false)` /
`Fable2FuncTraceSetSummaryEnabled(false)` / `Fable2FuncTraceFlush()`
(declared `extern "C"` in `src/core/fable2_func_trace.h`).

**Naming mode** (`FABLE2_FUNC_TRACE_SUBS_ONLY=1`): log only the unnamed
guest functions - names matching `sub_` + hex digits - dropping named
functions, the `__savegprlr_*`/`__restgprlr_*`/`__savevmx_*` register
helpers, and `xstart`. Both the text log and the summary honor it, so the
summary becomes a ranked list of the unnamed hot functions to name next.
Composes with the substring filter (`FABLE2_FUNC_TRACE_FILTER=82B9` narrows
to an address range).

**Launcher:** `fable2-functrace.cmd` (staged next to the exe) does the env
var dance for you and renames the previous session's log to
`fable2_func_trace_prev.log` first:

Arguments are keywords in any order (`subs` enables naming mode,
`trace`/`summary` pick which output file(s) are written, the backend word
picks the GPU path, anything else is the substring filter):

```
fable2-functrace.cmd                  D3D12, trace every call
fable2-functrace.cmd vulkan           Vulkan, trace every call
fable2-functrace.cmd subs             naming mode: only sub_<hex> functions
fable2-functrace.cmd subs LoadingScreen   naming mode + substring filter
fable2-functrace.cmd d3d12 82B9 subs  D3D12, address-range naming mode
fable2-functrace.cmd summary          summary file only, no trace log
fable2-functrace.cmd trace            trace log only, no summary file
```

With no `trace`/`summary` keyword both files are written (the default).
`summary` is the cheap option for long sessions - it avoids the multi-GB
sequential log while still accumulating the call counts.

It writes its own lightweight log (same pattern as `fps_probe.log`) rather
than the SDK spdlog logger, which would be far too slow at Fable 2's call
rate. To remove the feature: delete the `target_precompile_headers` block in
CMakeLists.txt + `src/core/fable2_func_trace.h` and rebuild.



## Allocator watcher (opt-in diagnostic)

The guest-allocator table watcher used to investigate main-menu startup races
is **not included by default** in Debug or Release builds. To compile it in
for a diagnostic build, configure CMake with `-DFABLE2_ALLOC_WATCH=ON`.
Only enabled builds start its background polling thread; it accepts a stop
request and joins at shutdown, before guest memory is destroyed. Do not enable
this for ordinary gameplay or performance measurements.

## Remote control (automated input channel)

`fable_2.exe` runs a localhost TCP **remote control server** so an external
automation harness can drive the guest gamepad over JSON-lines messages —
no human at the keyboard. Design doc: `plans/ai-remote-input-control.md`.

**Debug builds only.** This is a debugging/automation channel — it opens a
localhost TCP port and injects guest input — so it must not ship to players.
It is gated on the `FABLE2_REMOTE_CONTROL` macro, defined only for the Debug
build; Release / RelWithDebInfo builds compile out the pad driver + server
entirely (no port is opened and `fable2_control.py` is not staged).

- **Where:** `127.0.0.1:8791` by default (configurable, see below). One JSON
  object per line; every request gets exactly one response line; keep-alive
  connections are supported.
- **How:** a second synthetic pad driver
  (`src/input/remote_gamepad_driver.h`) is registered next to the keyboard driver,
  fed by the server (`src/input/remote_control_server.h`). It OR-merges with the
  human pads and is **not** gated on window focus, so the AI can drive the
  game while it's in the background.

Quick start (from the build dir, game running):

```
python tools\fable2_control.py ping
python tools\fable2_control.py press A --hold 120
python tools\fable2_control.py get-state
python tools\fable2_control.py script --file repro.json
```

Or speak the protocol directly (any language):

```
> {"cmd":"press","input":"RT","hold_ms":900}
< {"ok":true,"input":"RT","release_in_ms":900}
> {"cmd":"script","steps":[
      {"delay_ms":500,"op":"press","input":"LB","hold_ms":2000},
      {"delay_ms":1000,"op":"press","input":"A","hold_ms":80}]}
< {"ok":true,"duration_ms":2500}
```

`hold_ms` ≤ 0 (or omitted) means *hold until released/cleared*; a positive value
auto-releases after that many ms. In a `script`, each step's `delay_ms` is a **gap
since the previous step** on a single clock — the example presses LB at t=500 (held to
t=2500) and A at t=1500 (held to t=1580), so `duration_ms` (when the last input stops
being active) is 2500.

Commands: `ping`, `info`, `auth`, `press` (`input`, `hold_ms`, `value`),
`release`, `stick` (`input` = `StkLx`/`StkLy`/`StkRx`/`StkRy`, `value`,
`hold_ms`), `state` (sticky baseline: `buttons[]`, `triggers{LT,RT}`,
`stk{lx,ly,rx,ry}`), `clear`, `script` (atomic timed sequence), `get_state`,
`game_state` (current boot/menu state, see below), `screenshot` (`path`; save
the current game frame as a PNG, see below), `cvar` (get/set any
cvar by name), `enable`/`disable`. Input names match the
keyboard-gamepad vocabulary (`A`/`B`/`X`/`Y`, `LB`/`RB`, `LT`/`RT`,
`Up`/`Down`/`Left`/`Right`, `Start`/`Back`, `L3`/`R3`, stick direction
shorthands). `StkLy` positive = forward (Fable 2 convention). `script` is a
single atomic message, so a repro sequence runs with no network round-trips
between steps.

### Game state

`game_state` reports which boot/menu screen the game is on so the AI can
navigate and verify its actions:

```
python tools\fable2_control.py game-state
> {"cmd":"game_state"}
< {"ok":true,"state":{"code":2,"name":"PressAScreen"}}
```

The classifier samples once per second on a background thread (independent of
the render loop, so it keeps working during the movie, which renders video with
no UI text) and reports one of:

| State | Meaning |
|---|---|
| `?` (Unknown) | Arena not mapped yet / undetermined (first ~1 s). |
| `PreMainMenu` | Splash / intro, before the "Press A" prompt. |
| `PressAScreen` | The "Press A to start" prompt is on screen. |
| `MainMenuMovie` | The idle movie is playing (no prompt / menu). |
| `MainMenu` | The main menu (reached by pressing A on the prompt). |

This follows the game's own state machine:
`PreMainMenu →(time)→ PressAScreen →(time)→ MainMenuMovie →(time)→
PressAScreen`, with `PressAScreen →(A)→ MainMenu` and
`MainMenuMovie →(A)→ PressAScreen`. The prompt vs. the menu are visually
indistinguishable (same element-list draw rate), so the transition into the
menu is driven by the A-press, observed on the final merged pad state (remote +
keyboard + physical — it works no matter which input drives A) and latched with
its exact timestamp. State changes are logged to the in-game logger
(`REXSYS_INFO`, same channel as the remote control) on transition only, and
every sample is written to `fable2_state_probe.log` when
`FABLE2_STATE_PROBE=1` is set.

### Screenshot

`screenshot` saves the **current game frame** as a PNG so the AI harness can
see what the game is showing:

```
python tools\fable2_control.py screenshot shots/after_jump.png
> {"cmd":"screenshot","path":"C:/shots/after_jump.png"}
< {"ok":true,"path":"C:/shots/after_jump.png","width":1280,"height":720,"bytes":2765798,"avg_luma":120}
```

`path` (required, UTF-8) is where the PNG is written; parent directories are
created automatically. The frame is read **in-process from the renderer's guest
output texture** — the exact frame the game just rendered, in GPU memory at the
guest's native resolution (1280x720 for Fable 2) — not from the desktop. So the
picture is the complete game frame even while other windows cover it or the game
is unfocused, and occluding windows never appear in it. Works with both the D3D12
and Vulkan GPU plugins (each implements `CaptureGuestOutput`). Each capture
blocks the connection for a few hundred milliseconds. Implementation:
`src/diagnostics/fable2_frame_capture.h`.

Config (`[remote]` in `fable2_config.toml`): `enabled` (default `true`),
`host` (`127.0.0.1`; `0.0.0.0` exposes it on all interfaces), `port`
(`8791`; if busy, `+1..+9` are tried, the bound port is logged at startup),
`token` (empty = no auth; when set, each connection's first line must be
`{"cmd":"auth","token":"..."}`). Every received command is logged (audit
trail) to `logs/`.

## Building

Everything the build needs is either in this repo or auto-fetched — **no
hardcoded paths**. Prerequisites (all standard tools):

- **CMake** ≥ 3.25 (on PATH)
- **LLVM** (clang/clang++/lld) — on PATH or the default `C:\Program Files\LLVM` install
- **Ninja** (on PATH, `%USERPROFILE%\bin`, or the WinGet package dir)
- **Internet** on first build — `build.cmd` auto-downloads the prebuilt
  ReXGlue SDK v0.10.0 (~100 MB) from the
  [official release](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
  into `out\tooling\rexglue-sdk-0.10.0`. A compatible official SDK on PATH
  is used instead if present. Downloads never replace the source submodule.
- **Python and Git** for code generation and source-SDK preparation; **Visual
  Studio Build Tools / Windows SDK** (x64 developer shell) for the native build.

```bat
build.cmd
```

This is the one supported Windows build: the Release game, matched runtime,
and self-contained launcher are installed in `out/game`. On first run,
`build.cmd` builds the launcher and opens its ISO picker if game content is
missing. It extracts `out/game/default.xex` and `out/game/data`, then the
same command resumes the native build. Start `out/game/Fable2Launcher.exe`;
`fable_2.exe` is beside it. CMake intermediates stay in `out/build`, while
rebuilds preserve game content, settings, and saves.

The first `build.cmd` run fetches the official SDK if needed under `out/tooling`.
The Windows build uses the pinned, patched SDK from source; see
[runtime build requirements](docs/RUNTIME_FIXES.md). Game content is required
for code generation; the launcher extracts it from your disc into `out\game`.

Manual/advanced setup (normally not needed):

```bat
tools\setup_sdk.cmd      rem Download the official codegen SDK package.
tools\setup_sdk_src.cmd  rem Initialize the pinned thirdparty\rexglue-sdk
                         rem submodule and prepare the same tracked patches
                         rem used by build_runtime_sdk.cmd.
```

Both backend builders use the **one pinned source tree** at
`thirdparty/rexglue-sdk`, prepared by `tools/prepare_runtime_sdk.py`.
The official binary SDK is retained only for its compatible codegen tool;
`build.cmd` selects matching source-built native headers, libraries and
renderer DLLs. No independent `rexglue-sdk-src` checkout or obsolete
`rexglue-sdk-local.patch` application is required.

- **One build configuration:** `build.cmd` always builds the Windows x64 Release game using Ninja and Clang into `out\build\win-amd64-release`, then places executable files in `out\game`. No user-facing build-mode or target selection is needed.
- Codegen uses the tracked `fable_2_manifest.toml` with `[entrypoint].file_path = "out/game/default.xex"`. ReXGlue may regenerate `generated/rexglue.cmake` during codegen, but it continues to use that same manifest. No duplicate XEX, generated override manifest, or hard link is needed.
- A full clean build recompiles ~291 generated translation units (60,462 guest functions).

## Vulkan renderer

The SDK ships a D3D12 Xenos plugin by default on Windows; its Vulkan backend
is built separately from this project's pinned, patched source submodule at
`thirdparty/rexglue-sdk`. The Vulkan build enables both renderer backends.

**Stage 1 — foundation (DONE, verified).** A self-contained Vulkan pipeline that creates a window,
inits Vulkan, and presents a clear color, proving the whole stack on a real GPU:

```
cmake --build out\build\win-amd64-release --target fable_2_vulkan_smoke
out\build\win-amd64-release\fable_2_vulkan_smoke.exe   # -> stable blue window
```

Pipeline: dynamic `vulkan-1.dll` loader (no import lib needed) → `VkInstance` → Win32 surface →
device (graphics+present queue) → swapchain (3 images) → render pass → per-image framebuffers +
command buffers (clear-color subpass) → semaphores/fences → present loop. Logs to
`vulkan_smoke.log` next to the exe.

Key implementation notes (hard-won):
- **Window via the SDK's UI framework, not raw Win32/SDL.** The window is created with
  `rex::ui::SDLWindowedAppContext` + `rex::ui::Window` from the *shared* `rexruntime.dll` — the
  same mechanism the game uses. Raw `CreateWindowExW` and the *static* SDL build both fail to
  create/init a window here; only the shared-SDL path (the `rex::ui::*` exports) works. The target
  links the `rexruntime.lib` import lib and stages `rexruntime.dll` next to the exe.
- `#define NOMINMAX` before `<windows.h>` is required (the SDK's `math.h` uses `std::min` /
  `std::numeric_limits::max`, which the Windows `min`/`max` macros would break).
- Image views backing the swapchain **must outlive the framebuffers** (destroying them early makes
  the driver fault in `vkCmdBeginRenderPass`).
- Vendored Khronos headers live in `thirdparty/vulkan` + `thirdparty/vk_video` (Vulkan 1.3.282).

**Stage 2 — render the game via Vulkan (in progress).** Both the
D3D12 and Vulkan builds use the pinned, patched SDK source and produce
independent runtime/plugin snapshots. The Windows launcher checks provenance
and SHA-256 hashes before running each pair:

```bat
build.cmd       rem Build the game and matched D3D12 SDK.
tools\build_sdk_vulkan.cmd      rem Build and snapshot the Vulkan-enabled SDK.
build.cmd       rem Stage both verified pairs, even if exe is unchanged.

out\build\win-amd64-release\fable2.cmd          rem D3D12 (default).
out\build\win-amd64-release\fable2.cmd vulkan   rem Vulkan.
```

The D3D12 pair is taken from
`out/tooling/runtime-sdk/win-amd64/bin`; Vulkan is independently captured at
`out/tooling/vulkan-sdk/win-amd64/bin` **immediately after its build**. Unlike
the old approach, neither pair is read from the SDK's shared `source/out`
directory during game staging. The executable's `renderers/d3d12` and
`renderers/vulkan` directories each hold the matching DLLs and a
`renderer-pair.json` with the SDK commit, applied patch fingerprint, compiler
identity, configuration and DLL hashes.

`fable2.cmd` launches from a uniquely named, temporary instance directory,
rather than overwriting `rexruntime.dll` or `rexgpu-xenos.dll` next to the
main executable. The game uses the original game-content root; saves, cache,
and log directories are linked to the main game directory. This prevents
concurrent renderer selections from replacing DLLs underneath one another.
The direct `fable_2.exe` entry point remains the normal D3D12 configuration.

Vulkan is optional. If its SDK snapshot has not been built, D3D12 still
stages, and selecting Vulkan fails with an actionable error. If
`vulkan-1.dll` is not locally detected, no invalid CMake copy command is
generated; Windows can resolve its driver-installed loader instead.
The older `fable2.cmd prebuilt` mode is deliberately disabled because the
current source-built host does not have a verified ABI match to that runtime.
`fable2-uncapped.cmd` and `fable2-functrace.cmd` continue to call the
backend-selecting launcher.

## Manifest highlights (fable_2_manifest.toml)

Migrated from the XenonRecomp config (`XenonRecomp/fable2.toml`):

- 87 manual function boundaries. ReXGlue forbids the overlapping boundaries XenonRecomp allowed: three nested pairs were trimmed to end where the inner function begins, and `0x82C8D3F0` keeps its original full size with the nested `0x82C8D4E8` entry removed (the gap-fill otherwise extended the outer and emitted an illegal cross-function `goto`). Original sizes are recorded in the file's comments and in `XenonRecomp/fable2.toml`.
- 13 "gap seed" function entries (no size — the scanner sizes them) for branch targets the auto-discovery never registered: a chain of 8-byte thunks at `0x82C000F0`–`0x82C00128` plus stragglers (`0x82BEA25C`, `0x82CD7948`, `0x82C14D58`, `0x82F279D8`, `0x82E7E4F8`, ...).
- `setjmp_address = 0x83000200`, `longjmp_address = 0x82CA9260`, and two `[[invalid_instructions]]` data-table skips, carried over verbatim.
- The XenonRecomp register save/restore helper addresses (`restgprlr_14` etc.) have no ReXGlue equivalent and are preserved only as comments.
- Known runtime risk: codegen logs a handful of non-fatal "Unresolved conditional branch" warnings (e.g. `0x82C8D408`, `0x82C99E74`, `0x82C9A0BC`); those paths emit `REX_FATAL` if hit.

## Function naming rule (important)

A manifest `name` becomes a C symbol: codegen emits the function body as
`__imp__<name>` (extern "C"). A name must therefore NOT equal an XAPI export
of `rexruntimed.dll` (the DLL exports `__imp__<XAPI name>` for every
xboxkrnl/xam hook). If it does, the local definition silently shadows the
DLL import at link time (no linker error), and the generated import-thunk
registration (`fable_2_register.cpp`) routes XAPI calls into the guest
function body instead of the SDK kernel hook. That is what broke audio when
the game-internal KeWait re-implementations were named
`KeWaitForSingleObject` / `KeWaitForMultipleObjects`: the xboxkrnl import
thunks (0x832B28AC / 0x832B2CDC) started landing in the guest wrappers,
whose timeout field is milliseconds while kernel callers pass 100ns units
(0 = infinite). They are named `..._Guest` for this reason — keep it that
way. Check before committing name changes:

```
python tools/check_manifest_collisions.py
```
