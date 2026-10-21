# Windows launcher

A .NET 8 WPF settings launcher for verified GOTY USA/Europe and German GOTY
dumps. It validates the original XEX and content markers, then checks the
native build descriptor. Unknown revisions and known mixed retail/TU1 content
are rejected; original game files are never modified.

## Extracting from an ISO

"Extract from ISO" opens a file picker for a raw Xbox 360 disc image (`.iso`)
and extracts straight into the folder the launcher is running from (next to
`fable_2.exe`) — there is no destination picker. The ISO content is written in
two phases:

1. **`default.xex` only** — extracted and SHA-256-hashed first.
2. **The remaining disc** (`data`, `nxeart`, `$SystemUpdate`, …) — written if
   the hash matches a known-good build in the version catalogue
   (`GameCompatibilityInspector.CheckHash`, same catalogue the launcher uses
   to validate game folders). If the hash does **not** match, extraction pauses
   on the progress screen with the reason and a red **Extract Anyways** button:
   click it to keep the XEX and write the rest of the disc, or press **Stop**
   to bail out (which deletes the ~21 MB XEX so nothing partial is left behind).

On success the launcher's folder remains the single game installation.
The
GDFX/ISO 9660 readers come from the shared `X360Extract` library
(`tools/x360extract/lib`, referenced as a project dependency), so the output is
byte-for-byte identical to the standalone `x360extract` tool; STFS containers
are copied raw.

Settings: 720p/1080p/1440p/4K output, 1x–4x internal render scale, anisotropic
filtering (game default through 16x), none/FXAA/FXAA Extreme, VSync,
windowed/borderless/exclusive fullscreen and 30/60/120/144/165/240/unlimited FPS. Output size
is separate from the original 720p guest mode. The FPS options need the
matched source-built Release runtime described in
[the runtime guide](../docs/RUNTIME_FIXES.md). Higher rates are not a promise
of correct timing in every scene.

The **Advanced Options** button in the top bar opens a second page for the
recomp's own settings (stored in `fable2_config.toml` next to `fable_2.exe`,
separate from the engine's `fable_2.toml`):

- **Keybinds** — the keyboard→gamepad map as a list of editable rows (host key
  → guest input, with add/remove). Mouse look (via the gamepad right-stick path) and mouse sensitivity are also
  here.
- **Patches** — a checkbox per recomp-level patch (unlock website/CE, skip
  intro videos, disable motion blur, realtime texture morphing, hero/dog
  texture readback, dog-fur depth bias).
- **Performance** — the hot-function yield-batching factor.

The Save button writes only the managed keys back to `fable2_config.toml`
non-destructively: it updates keys in place (keeping indentation, spacing and
inline comments), appends any missing keys or sections, leaves every other key
and section untouched, and makes a one-time `.launcher-backup` before the first
write. Keybind and patch changes take effect the next time the game launches.

Dropdowns use dark text on a light background, including the selected item.
There are no texture/font replacements, remaster switches or save editors.
The launcher and game use an original project-owned book/tree icon, not
extracted game art; see [asset provenance](../assets/README.md).

Build from the repository root with `build.cmd`. It publishes the
self-contained launcher to `out\game\\Fable2Launcher.exe` before the
native build. If the original disc content is absent, the build starts the
launcher in extraction/setup mode. Choose your own ISO; the launcher returns to
the build command after successful extraction.

There is **one** game directory: `out\game` (the launcher's own
directory), containing `Fable2Launcher.exe`, `fable_2.exe`,
`default.xex`, `data/`, runtime DLLs, settings and saves.
Neither folder browsing nor an alternative publish mode is supported.
The published launcher bundles the .NET 8 Desktop Runtime.

`launcher-settings.toml` stores only the managed preferences. The launcher
reads existing `fable_2.toml` first for legacy configurations, then overlays
those preferences so runtime rewrites cannot reset output resolution or render
scale. Save/start writes both files. Unknown engine settings are preserved,
and the first engine-config write creates a one-time backup. Saves, caches and
logs remain beside the native EXE, not in the original dump.

F3 in the game shows the guest-swap FPS counter in Release.
`StartWithoutAudio.cmd`, staged with the native EXE, exercises the clocked
silent fallback without disabling any Windows device. Exit existing game and
launcher processes before running that test.


Launcher overview supplied by the tester (before the high-refresh presets were added):

![Launcher overview](../docs/screenshots/launcher.png)
