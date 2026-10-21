# Adaptive keyboard/controller button prompts

Fable II renders some Xbox button icons from inline UTF-16BE markup such as
`Press <a_img> to start`. The recomp now attempts to replace recognized
*writable source strings* with compact labels from the active ReXGlue keybinds,
for example `Press [  E  ] to start`. This is a best-effort first phase, not a
universal game-asset replacement or an atlas/icon redesign.

## Behavior

- Default: `--ui_prompt_mode auto`. The native input merger tracks meaningful
  keyboard/mouse or physical-controller activity and switches the display
  accordingly. Idle pads and held inputs do not repeatedly win by polling order.
- `--ui_prompt_mode keyboard`: force keyboard labels.
- `--ui_prompt_mode controller`: preserve original Xbox text and icons.
- These options can also be set in the SDK's `fable_2.toml`.
- Supported inline tags: `<a_img>`, `<b_img>`, `<c_img>`, `<d_img>`,
  plus `<x_img>` and `<y_img>` aliases. The `c/d` to X/Y mapping follows
  the game's ordered tag table and still needs visual confirmation.
  Unknown tags are unchanged. A key needs a compact name (max five characters
  inside brackets); otherwise the original icon is preserved.
- Labels come from ReXGlue's live `keybind_a/b/x/y` settings, which the launcher
  populates using `keyboard_gamepad_map`. A normal usable keyboard key is
  preferred over a mouse binding; lone modifiers are skipped because the
  current native driver cannot activate them as standalone bindings.
- No game files, save files, fonts, shader code or texture atlases are edited.

## How it works and limits

The UI-markup translator is pure C++. Its seven-character keycaps preserve
exactly the length of the original inline image tags, avoiding the early NUL
and stale length mismatch that the first version introduced.

A background worker now starts after XEX loading but **before guest UI setup**,
not when the render loop first executes. It scans the title prompt neighborhood
(`0x42600000..0x42700000`) frequently and explores the broader UI heap at a
slower cadence. The worker is joined on shutdown, before guest memory unmaps.
It scans only committed,
writable Fable II front-end heap memory between `0x40000000` and
`0x44000000`. It looks for bounded UTF-16BE strings with known markup and
replaces them with shorter text, without adding bytes beyond the original
string storage. Before any restoration, it verifies the address still
contains exactly the bytes it wrote, so it does not intentionally overwrite
heap allocations that have since changed.

This works only for source strings that still exist as writable text and whose
UI layout is refreshed from them. Some prompts may be composed only once,
stored in other memory, represented as literal localized text, or drawn as
baked textures. Those prompts retain Xbox glyphs. In particular, this first
phase is *not* a guarantee that every in-game prompt changes. The executable
and renderer must be rebuilt with the matched SDK runtime for active-device
detection. Actual screen behavior has not been measured in a Windows game
session here.

If the displayed prompts are incorrect, restore original behavior with
`--ui_prompt_mode controller`. The SDK changes are isolated in
`thirdparty/rexglue-sdk-adaptive-prompts.patch`; the text replacement lives in
`src/diagnostics/fable2_button_prompts.h`.

Follow-up for comprehensive replacements: identify and hook the actual
markup-to-inline-sprite resolver during UI layout, then optionally replace
button sprites with true Fable-styled dynamic keycaps. That would also reach
prompts whose source string is not retained in writable heap memory.

## Diagnosing whether the strings were reached

Check `fable2_button_prompts.log` next to the game. The scanner reports
`known_strings`, `replaced`, and `tracked`, plus changes in active device
and the selected keys. If `replaced = 0` with the title screen visible, the
scanner did not find a supported source string or the string was already
compiled into different UI state. If `replaced > 0` while Xbox images still
appear, the guest renderer is using previously laid-out elements rather than
rereading the text; then the next step is texture/inline-element interception,
not more scanning. Rebuilding the full matched runtime is required if the
active input device never changes in the log.

**Do not interpret the log as proof of visual replacement.** This work has no
verified Windows UI capture. The existing project research records the title
prompt's per-element render chain (`UITextPrompt_RenderElement` and
`0x82C12F18`); a future native render hook or texture-atlas replacement will
be necessary for cached/baked prompts. Other ReXGlue projects have implemented
hash-based texture dumps and replacements, but Fable II's SDK currently lacks
that subsystem, and no known Fable II button-atlas hash exists in this repo.
