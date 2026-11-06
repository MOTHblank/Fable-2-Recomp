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
specific visual branch and prefixes the label with the live binding, such as
`[E] Travel`. Paired widgets use the same conservative single-visual-wrapper
search as the texture-only fallback, so a material/texture node can still be
hidden through its dedicated wrapper without hiding the sibling label or a
different component. This native GameFace-text path is preferred over a host
keycap whenever it is safe. If a presentation initially exposes only the icon,
the texture-only host fallback may be used temporarily; a later full discovery
pass can promote that same adapter-owned hidden visual to native GameFace text
as soon as its writable label appears, without flashing the Xbox artwork back
on during the handoff. Texture paths ending in `icon_button_*.tex`, named
`AButton/BButton/XButton/YButton` widgets, and the documented
`Material2270` A-button material are recognized. It stores the original
visual attribute and text in the presentation's Lua state. Switching back to
controller mode restores both.

This gives non-string UI a practical PC representation without globally
replacing `icon_button_a.tex` or repainting every use of `Material2270`.
If no safe text association exists, known controller artwork proceeds to the
texture-only keycap fallback below; truly ambiguous visuals remain native.

GameFace `behavior` elements are explicitly traversal-only. Contract/event
nodes such as the reverse-engineered `PressA`, `OnPressB`,
`FillWorldIcon`, and `FillWorldIconButton` are never rewritten, hidden, or
used as overlay targets even if a future artwork alias shares their name. The
adapter acts on the separate visual/material elements instead; this prevents
prompt presentation work from disabling the interaction behavior itself.

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
pads do not participate. Held buttons and stationary sticks are not repeated
activity, so they cannot steal prompt ownership back from a mouse that was
used afterward.

Per-device comparison history belongs to the current SDK `InputSystem`
instance and is erased when a device disappears or input shuts down. A
reconnected pad/device ID therefore starts with a clean baseline instead of
inheriting stale buttons or stick positions from its previous lifetime.

Mouse-wheel input does not invent a guest-gamepad binding; it only counts as
unambiguous physical mouse activity for last-used-device ownership.

Focus changes and pointer-lock/capture transitions may produce one synthetic
SDL mouse-motion sample. The MnK driver treats the first motion sample after
such a transition as an ownership **baseline**: gameplay mouse routing still
receives it, but it cannot flip `input_prompt_keyboard`. A subsequent
physical motion sample uses the normal one-pixel accumulator. Explicit key,
mouse-button and wheel activity still switches immediately.

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
mouse-motion accumulator, the `OnMouseWheel` ownership hook, and
disconnect-time prompt-history cleanup, so a partially updated SDK cannot
silently lose the stable switching behavior.

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
same logical action. Safe visual hiding now tries the GameFace attributes
`opacity`, `active`, then `diffuse.a`. All three are exposed by the
verified GameFace element API and are restored only while their current value
still exactly matches the adapter-applied value. If the icon itself lacks all
three, the adapter may climb up to three wrapper levels, but only through a
**single-visual-wrapper chain**: sibling behavior/material/texture helpers are
allowed, while sibling text/components/groups stop the climb so unrelated UI
can never be hidden with the icon.
Visibility ownership is attribute-specific: if the adapter owns `opacity=0`
but the game independently sets `active=false` or `diffuse.a=0`, that
game-owned hide still suppresses the host keycap. Zero `scale.x`/`scale.y`
is also treated as a game-owned visibility state, so animated/scaled-away
prompt branches do not leave a replacement floating behind. Wrapper promotion
fails closed if the parent element type cannot be queried. Elements exposing
none of these safe visual attributes remain native.

If the presentation's pivot semantics need a small correction, the host overlay
can be tuned live with `ui_prompt_overlay_offset_x` and
`ui_prompt_overlay_offset_y` (both default to 0 and accept -128..128 pixels).
Modifier chords are rendered as separate keycaps, for example
`Shift+Up -> [S] + [Up]`, while in-GameFace text remains compact (`S+Up`).
Mouse side buttons are presented as `M4` / `M5` rather than the SDK's
internal `XMB1` / `XMB2` binding names.

## Legacy UITextPrompt title fallback

The verified title-screen prompt has a separate renderer:
`UITextPrompt_Render -> UITextPrompt_RenderElement`. By that stage the
`Press <a_img> to start` source string may already have been discarded and
the renderer receives only laid-out element/drawable data.

`src/diagnostics/fable2_legacy_prompt.h` bridges that specific path.
The scanner now publishes both the localized layout counts and the exact
0-based baked element ordinal/action of the first verified `<x_img>` tag.
A stock seven-character image tag occupies one laid-out sprite, while the same
seven code units become seven ordinary glyphs after the native keycap rewrite.
`UITextPrompt_Render` is bracketed per invocation and its element calls are
counted, so the adapter can prove whether the rendered layout actually consumed
the source rewrite.

Every prompt element still runs through the complete stock
`UITextPrompt_RenderElement` path, including visibility, animation and
bookkeeping. The manifest now names its indirect drawable vtable slot 8 target,
`UITextPrompt_DrawableRender` at `0x82C12F18..0x82C12F78` (0x60 bytes).
After one frame positively matches the source-derived baked layout, the adapter
records the **exact guest element pointer** seen at the source-derived image
ordinal. Only when that same pointer reaches that same ordinal again in
keyboard mode is its final drawable-leaf call skipped. Text glyphs, unknown
layouts, different element objects and controller mode always continue through
the stock renderer. No material, texture bank, tag table or interaction
behavior is modified.

The legacy host keycap is allowed only when all of the following are true:

- keyboard/mouse is the active prompt device;
- `ui_prompt_legacy_fallback = true`;
- one `UITextPrompt_Render` invocation matches the localized title prompt's
  expected baked/rewritten structure;
- that invocation has just rendered a visible drawable;
- the observed element count still matches the baked controller-sprite layout;
- the conservative cover itself is refreshed only by the exact
  `UITextPrompt` item learned while the verified `0x4266xxxx` title source
  is actively tracked with a baked-layout count;
- for renderer-level sprite suppression, the source scanner also identified
  the image-tag ordinal, and both the current prompt item pointer and image
  element pointer exactly match the stricter identity learned from a confirmed
  baked title frame.

As soon as a later title layout matches the expanded keyboard-glyph count, the
fallback disables itself. The visibility pulse expires after roughly 110 ms,
so it follows the title prompt's original blink instead of remaining
continuously visible. When the exact sprite is safely suppressed, the host
draws a normal compact keycap. The named drawable leaf also exposes the
caller's already-prepared scratch object; the adapter inspects only its first
96 bytes for an adjacent float/integer canvas-coordinate pair within ±160 X
and ±120 Y of the verified authored `(640,560)` title position. Because that
scratch layout is inferred rather than decoded, one plausible frame is not
enough: the **same prompt item and image element must report coordinates within
2 canvas units on two consecutive confirmed baked frames** before runtime
geometry replaces the authored anchor. Otherwise `(640,560)` remains the
fallback.
When only count-level evidence exists, the larger opaque cover is retained
instead of risking a guest-renderer skip. `ui_prompt_legacy_offset_x` and
`ui_prompt_legacy_offset_y` (default 0, range -128..128) still allow live
alignment correction.

This remains a targeted fallback rather than a global controller-texture
replacement. Switching back to a controller immediately uses the stock
renderer. Other `UITextPrompt` invocations cannot inherit title suppression
merely by sharing an element count: the current element must also match the
source-derived ordinal **and** the exact guest pointer learned from the
confirmed title layout.

## Remaining hard boundary

Ordinary GameFace controller artwork and the verified legacy title prompt are
now covered. The title path also removes its own baked face-button draw when
source-derived element identity is available. The remaining unknown cases are
narrower:

- other legacy inline-sprite/text item types for which no stable
  action/element-to-screen-position contract has yet been identified. Current
  captures prove `UITextPrompt_Render` for the title item; other observed
  screens used `UIText_RenderCurrent`, so the title hook is not assumed to be
  a generic prompt renderer;
- presentation artwork that never appears in the traversable GameFace scene;
- GameFace icon nodes for which neither the element nor any conservative
  single-visual-wrapper chain exposes `opacity`, `active`, or `diffuse.a`.

Those remain native rather than being assigned the title prompt's fixed
position, interpreting UIContract behavior names as visual controls, or
globally replacing a shared Xbox texture. A future targeted hook at the
`0x820D7B50` tag resolver/layout boundary could generalize the legacy
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
