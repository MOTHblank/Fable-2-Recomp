# Adaptive keyboard/controller button prompts

Fable II uses more than one prompt path. Some button prompts originate as
writable UTF-16BE markup such as `Press <a_img> to start`; others are already
GameFace elements backed by controller icon materials such as
`icon_button_a` / `AButton`. The recomp now adapts both paths and restores
the original Xbox presentation whenever a controller becomes the last-used
physical device.

## Behavior

- Default: `--ui_prompt_mode auto`. The SDK tracks the last meaningful
  physical input. Keyboard presses, mouse buttons and real mouse movement select
  keyboard/mouse prompts immediately; controller button/trigger/stick activity
  selects Xbox prompts. Synthetic debug pads do not steal prompt ownership.
- `--ui_prompt_mode keyboard`: force keyboard labels.
- `--ui_prompt_mode controller`: preserve the native Xbox presentation.
- These options can also be set in the SDK's `fable_2.toml`.
- Labels come from the live ReXGlue `keybind_*` cvars, which are populated
  from `keyboard_gamepad_map`. The display helper prefers a usable keyboard
  alternative over a mouse button and supports compact modifier chords such as
  `Shift+Up -> S+Up`.
- A display label is intentionally capped at five characters. If no safe compact
  label exists, that prompt keeps its original Xbox artwork rather than showing
  a misleading or truncated key.
- No game data, save files, fonts, shaders or texture banks are edited.

## Layer 1: writable inline prompt markup

`src/diagnostics/fable2_button_prompts.h` runs a bounded worker after XEX
loading and before the front-end finishes constructing its UI. It scans only
committed writable memory in the known front-end heap and rewrites recognized
seven-character tags to same-length text keycaps.

Currently verified tag vocabulary is the game's face-button table:
`<a_img>`, `<b_img>`, `<c_img>`, `<d_img>` plus the `<x_img>` and
`<y_img>` aliases. The replacement keeps the original seven UTF-16 code units
(e.g. `<a_img> -> [  E  ]`), so cached string-length fields do not become
inconsistent.

Every write is tracked with its original bytes. Restoration occurs only when
the guest allocation still contains exactly the bytes written by the recomp,
which avoids writing through stale/reused heap addresses.

## Layer 2: live GameFace icon widgets

`src/core/fable2_gui_lua.h` now contains a reversible prompt adapter for
already-composed GameFace UI. It walks each live presentation and recognizes
common controller-icon names for A/B/X/Y, shoulders, triggers, Start/Back,
stick clicks and D-pad directions. It also recognizes the documented
`AButton` / `BButton` / `XButton` / `YButton` style widget names.

The adapter does **not** blindly suppress controller textures. It changes an
icon only when it can find a nearby writable text label (for example the
documented `AButton` + `ButtonText` pairing). In keyboard mode it hides that
specific icon and prefixes the label with the live binding, such as
`[E] Travel`. It stores the original icon visibility and text in the
presentation's Lua state. Switching back to controller mode restores both.

This gives non-string UI a practical PC representation without globally
replacing `icon_button_a.tex` or repainting every use of `Material2270`.
Ambiguous/baked elements with no safe text partner deliberately stay native.

The prompt worker re-broadcasts the current mode periodically. Broadcasts are
coalesced, and the Lua side is idempotent for an unchanged presentation/mode,
so newly created HUD/menu states can receive the current device choice without
polling and rewriting the same elements every frame.

## Seamless last-used-device switching

The active-device signal lives in the SDK rather than in Fable-specific UI
code. `InputSystem` still detects meaningful physical controller activity
from actual pad state changes. The MnK driver additionally marks keyboard/mouse
ownership directly from host events:

- key down;
- mouse button down;
- non-zero mouse movement.

The direct mouse-camera path therefore switches prompts even though its raw
motion bypasses the emulated right stick. This is important for the hybrid
native-camera mode: simply moving the mouse is enough to make the UI read as a
PC UI again.

The current pinned SDK revision is checked by
`tools/prepare_runtime_sdk.py` for `MarkKeyboardPromptDevice`, so a partially
updated SDK cannot silently lose this behavior.

## Remaining hard boundary

Fable II still contains artwork that may be fully baked into a presentation,
have no writable companion label, or be generated through an unidentified
inline-sprite resolver. Those cases are intentionally left as Xbox artwork
rather than guessed at. Replacing those last assets with true Fable-styled
dynamic keyboard keycaps would require either a targeted hook in the
markup-to-sprite resolver or a texture/material substitution path with
dedicated keycap assets.

That is now a presentation-quality extension rather than a requirement for
last-device switching: the current implementation covers retained source
markup and safely adaptable live GameFace button widgets.

## Diagnostics

`fable2_button_prompts.log` reports source-string scanning and active prompt
mode/key changes. `known_strings`, `replaced` and `tracked` refer to the
writable-markup layer; GameFace widget replacements are handled separately by
the GUI bridge.

If `replaced > 0` while a particular Xbox image remains visible, that image
is likely already represented as a cached/baked GameFace element. If the
element has a safe sibling text label, the GameFace adapter can handle it; if
not, preserving the native icon is intentional.

Actual visual coverage still depends on the presentation currently loaded.
This source work has not been verified here against a captured Windows game
session.
