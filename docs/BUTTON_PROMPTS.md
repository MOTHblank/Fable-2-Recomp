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
- The recomp's own `fable2_config.toml [input]` now exposes `prompt_mode`,
  `prompt_overlay_fallback`, `prompt_legacy_fallback`, and the overlay
  alignment offsets. SDK `fable_2.toml`, environment variables, CLI values
  and live console changes keep their normal higher priority.
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

The same bounded worker now also handles conservative fixed-width literal
instructions using the verified verbs `Press`, `Hold`, `Tap`, and
`Select`. These use the game's own font and are rewritten only when the
current keyboard label fits in the original controller token width. For
example, `Press A to confirm` can become `Press E to confirm`, and the
observed book-footer `Select A` can become `Select E`, without changing the
allocation. A mouse-only `X -> LMB` mapping cannot fit in a one-character
`X` token, so that source string is intentionally left untouched here and
may be adapted later by a relayout-capable GameFace path.

Every native source rewrite is required to preserve the **exact** UTF-16 code
unit count. The scanner will reject both growth and shortening rather than
changing a guest allocation or relying on stale cached lengths.

## Layer 2: live GameFace icon widgets

`src/core/fable2_gui_lua.h` now contains a reversible prompt adapter for
already-composed GameFace UI. It walks each live presentation and recognizes
common controller-icon names for A/B/X/Y, shoulders, triggers, Start/Back,
stick clicks and D-pad directions. It also recognizes the documented
`AButton` / `BButton` / `XButton` / `YButton` style widget names.

The adapter also rewrites exposed live `textstring` values. That catches
markup which survived into the presentation (`<a_img>`, etc.) and conservative
literal instruction forms such as `Press A`, `Hold X`, `Tap RT`,
`Press Start`, or the verified book-footer form `Select A`. Unlike the
fixed-size heap rewrite, `setAttribute` can trigger
GameFace relayout, so these replacements may use the natural compact form
`[key]`.

The adapter does **not** blindly suppress controller textures. It changes an
icon only when it can find a nearby writable text label (for example the
documented `AButton` + `ButtonText` pairing). In keyboard mode it hides that
specific icon and prefixes the label with the live binding, such as
`[E] Travel`. Texture paths ending in `icon_button_*.tex`, named
`AButton/BButton/XButton/YButton` widgets, and the documented
`Material2270` A-button material are recognized. It stores the original icon
visibility and text in the presentation's Lua state. Switching back to
controller mode restores both.

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
- mouse wheel activity;
- mouse movement reaching one accumulated physical pixel while a controller
  owns prompt focus.

The direct mouse-camera path therefore switches prompts even though its raw
motion bypasses the emulated right stick. Mouse motion is accumulated only
while a controller owns the prompts and must reach one physical pixel before
switching, so fractional high-resolution/raw-input noise cannot make the UI
oscillate. Keyboard presses and mouse buttons still switch immediately.

Physical controller ownership has its own noise floor: stick activity must
clear the normal thumb deadzone and change by at least 3500 units, while
buttons and trigger-threshold crossings switch immediately. Synthetic debug
pads do not participate.

Mouse-wheel input does not invent a guest-gamepad binding; it only counts as
unambiguous physical mouse activity for last-used-device ownership.

GameFace changes are asynchronous because the adapter runs inside each live
presentation's Lua state. The host overlay therefore follows the **last
acknowledged GameFace state**, not merely the desired cvar state. On
keyboard -> controller, an existing keycap remains for the short handoff until
GameFace has restored the Xbox icon. On controller -> keyboard, the host waits
until GameFace has actually hidden the Xbox artwork before drawing the keycap.
The same handshake applies when `ui_prompt_overlay_fallback` is toggled.

Acknowledgement results encode keyboard/controller mode plus whether the
texture-only fallback was applied. Old/inactive Lua states may remain captured,
so a mismatched acknowledgement gets only a 300 ms transition grace rather
than the normal presentation lifetime. This avoids both one-frame blank/duplicate
prompts and multi-second stale keycaps from abandoned screens.

The source scanner, GameFace adapter and host overlay share
`KeyboardPromptModeActive()` as the single `auto / keyboard / controller`
decision.

The current pinned SDK revision is checked by
`tools/prepare_runtime_sdk.py` for `MarkKeyboardPromptDevice`, the
mouse-motion accumulator, and the `OnMouseWheel` ownership hook, so a
partially updated SDK cannot silently lose the stable switching behavior.

## Texture-only fallback

Some GameFace button elements expose controller artwork but no writable text
label. With `ui_prompt_overlay_fallback = true` (default), the adapter now
handles those separately instead of leaving them permanently Xbox-only.

For an unresolved known controller icon, the GameFace pass:

1. computes its live position through the parent transform chain on the authored
   1280x720 canvas;
2. hides that specific icon using its own (or immediate wrapper's) opacity /
   active attribute;
3. returns the action and canvas position to the host;
4. draws a compact, Fable-compatible dark/ivory keycap at that position through
   the existing ImGui foreground layer.

The host uses the same wide-window canvas transform as the game's HUD fitting
code, so these keycaps follow arbitrary aspect ratios instead of assuming a
fixed 16:9 framebuffer. Presentation reports expire automatically after roughly
two seconds, which prevents stale overlays surviving screen changes.

Unresolved icons retained by the adapter are refreshed separately from full
scene discovery. While keyboard mode and the overlay fallback are active,
their live GameFace transform/visibility is sampled at about 10 Hz, while the
full up-to-2500-element discovery/text-rewrite walk runs only about every
eighth refresh (~0.7–0.8 s). This lets animated HUD or world-following prompt
keycaps track their source without repeatedly traversing the entire scene.
If a game-owned ancestor becomes inactive or transparent, the host keycap is
suppressed immediately; the adapter's own hidden opacity/active attribute is
ignored for that visibility test.

The fallback remains last-device aware. Controller mode restores the GameFace
attributes and clears the host entries. Changing
`ui_prompt_overlay_fallback` also participates in the adapter signature, so
turning it off restores unresolved Xbox artwork on the next synchronization
rather than merely hiding the host keycap.

Known alternate/pressed assets such as `icon_button_a2` are normalized to the
same logical action. The fallback still refuses to hide an element when neither
it nor its immediate wrapper exposes a safe visibility attribute.

If the presentation's pivot semantics need a small correction, the host overlay
can be tuned live with `ui_prompt_overlay_offset_x` and
`ui_prompt_overlay_offset_y` (both default to 0 and accept -128..128 pixels).
Modifier chords are rendered as separate keycaps, for example
`Shift+Up -> [S] + [Up]`, while in-GameFace text remains compact (`S+Up`).

## Legacy UITextPrompt title fallback

The verified title-screen prompt has a separate renderer:
`UITextPrompt_Render -> UITextPrompt_RenderElement`. By that stage the
`Press <a_img> to start` source string may already have been discarded and
the renderer receives only laid-out element/drawable data.

`src/diagnostics/fable2_legacy_prompt.h` now bridges that specific path. It
always forwards to the original renderer first, then uses the documented
post-render element state only as a visibility/blink signal. No legacy draw
object, material, texture or element list is rewritten.

The scanner publishes the exact expected element counts from the localized
source string: a stock seven-character `<a_img>` tag occupies one laid-out
sprite, while the same seven code units become seven ordinary glyphs after the
native keycap rewrite. `UITextPrompt_Render` is bracketed per invocation and
its element calls are counted. This proves whether the rendered layout actually
consumed the source rewrite instead of assuming that a scanner hit happened
early enough.

The legacy host keycap is allowed only when all of the following are true:

- keyboard/mouse is the active prompt device;
- `ui_prompt_legacy_fallback = true`;
- one `UITextPrompt_Render` invocation matches the localized title prompt's
  expected baked/rewritten structure;
- that invocation has just rendered a visible drawable;
- the observed element count still matches the baked controller-sprite layout.

As soon as a later title layout matches the expanded keyboard-glyph count, the
fallback disables itself. The visibility pulse expires after roughly 110 ms,
so it follows the title prompt's original blink instead of remaining
continuously visible. It covers the already-baked A sprite at the authored
bottom-center title position. `ui_prompt_legacy_offset_x` and
`ui_prompt_legacy_offset_y` (default 0, range -128..128) allow live alignment
correction without a code change.

This is deliberately a cover/fallback, not a global controller-texture
replacement. Switching back to a controller makes it disappear immediately,
and other legacy prompt invocations cannot trigger the fixed title position
because their element counts do not match the localized title structure.

## Remaining hard boundary

Ordinary GameFace controller artwork and the verified legacy title prompt are
now covered. The remaining unknown cases are narrower:

- other legacy `UITextPrompt` / inline-sprite users for which no stable
  element-to-screen-position contract has yet been identified;
- presentation artwork that never appears in the traversable GameFace scene;
- GameFace icon nodes whose own element and immediate wrapper expose no safe
  visibility attribute.

Those remain native rather than being assigned the title prompt's fixed
position or globally replacing a shared Xbox texture. A future targeted hook at
the `0x820D7B50` tag resolver/layout boundary could generalize the legacy
fallback once the other call sites and their position data are identified.

## Scanner cadence and diagnostics

The worker prioritizes the title allocation without periodically walking the
entire front-end heap in one burst:

- `0x42660000..0x42670000`: 64 KiB title fast path, approximately every
  15 ms;
- `0x42600000..0x42700000`: 1 MiB title-neighborhood safety pass every
  120 ms;
- `0x40000000..0x44000000`: rolling 2 MiB slices every 60 ms, completing
  the 64 MiB fallback range in about two seconds.

Broad slices overlap by 320 bytes (the maximum supported 160 UTF-16 code
units), so a prompt crossing a slice boundary is still examined as one string.
This preserves the previous broad coverage while avoiding a single large
two-second scan burst.

`fable2_button_prompts.log` reports source-string scanning and active prompt
mode/key changes. `known_strings`, `replaced` and `tracked` refer to the
writable-markup layer; GameFace widget replacements are handled separately by
the GUI bridge.

If `replaced > 0` while a particular Xbox image remains visible, that image
is likely already represented as a cached/baked GameFace element. If the
element has a safe sibling text label, the GameFace adapter can handle it; if
not, the texture-only overlay fallback is the next layer.

For the title screen, the legacy fallback is now layout-driven rather than
scanner-timing-driven. A source rewrite that occurred too late remains covered
until `UITextPrompt_Render` actually reports the expanded keyboard-glyph
element structure.

Actual visual coverage still depends on the presentation currently loaded.
This source work has not been verified here against a captured Windows game
session.
