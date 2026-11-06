// Last-resort keyboard prompt bridge for Fable II's legacy UITextPrompt path.
//
// The ordinary adaptive-prompt pipeline rewrites writable <a_img> source
// markup before layout. If that source allocation has already disappeared,
// UITextPrompt_RenderElement receives only baked element/drawable data.
//
// Ordinarily this forwards to the original function and uses its documented
// post-render visibility state to pulse the host keycap overlay. Once the
// localized source scanner has identified the exact baked <x_img> element
// ordinal and a prior frame confirms the stale baked layout, keyboard mode may
// suppress only that one sprite element. All text glyphs and unknown layouts
// continue through the stock renderer unchanged.
#pragma once

#include <cstdint>

#include <rex/cvar.h>
#include <rex/ppc/context.h>

#include "fable2_button_prompts.h"
#include "fable2_prompt_overlay.h"

extern "C" void __imp__UITextPrompt_Render(PPCContext& ctx,
                                            uint8_t* base);
extern "C" void __imp__UITextPrompt_RenderElement(PPCContext& ctx,
                                                   uint8_t* base);

namespace fable2::legacyprompt {

inline bool PlausibleElement(uint32_t element) {
  return element >= 0x10000u && element < 0xE0000000u &&
         (element & 3u) == 0;
}

inline bool WasVisibleAfterRender(uint32_t element, uint8_t* base) {
  if (!PlausibleElement(element) || !base) return false;

  // TEXT_RENDERING_NOTES.md documents the early visibility rejection at
  // element+0xBC as value 2. The original build may use either a byte-sized
  // state or a word-sized field around this offset, so recognize both
  // representations. This is one bounded read from the element the original
  // function just used; unlike the old probes, there is no heap walking on
  // the render thread.
  const uint8_t state8 = REX_LOAD_U8(element + 0xBC);
  const uint32_t state32 = REX_LOAD_U32(element + 0xBC);
  if (state8 == 2 || state32 == 2) return false;

  // The original renderer skips elements without a drawable at +0x7C.
  return REX_LOAD_U32(element + 0x7C) != 0;
}

inline bool PlausiblyVisibleWithoutRender(uint32_t element, uint8_t* base) {
  if (!PlausibleElement(element) || !base) return false;
  if (REX_LOAD_U32(element + 0x7C) == 0) return false;

  // When suppressing the draw we intentionally do not run the element's
  // renderer, so +0xBC is the state left by the preceding frame. Treat an
  // explicit rejection as invisible and otherwise let the surrounding text
  // elements determine the frame's blink/activity pulse.
  const uint8_t state8 = REX_LOAD_U8(element + 0xBC);
  const uint32_t state32 = REX_LOAD_U32(element + 0xBC);
  return state8 != 2 && state32 != 2;
}

}  // namespace fable2::legacyprompt

extern "C" void UITextPrompt_Render(PPCContext& ctx, uint8_t* base) {
  fable2::promptoverlay::BeginLegacyTitleFrame();
  __imp__UITextPrompt_Render(ctx, base);
  fable2::promptoverlay::EndLegacyTitleFrame();
}

extern "C" void UITextPrompt_RenderElement(PPCContext& ctx, uint8_t* base) {
  const uint32_t element = ctx.r3.u32;
  const uint32_t ordinal =
      fable2::promptoverlay::CurrentLegacyTitleElementOrdinal();
  const bool suppress =
      fable2::promptoverlay::ShouldSuppressLegacyTitleElement(ordinal);

  bool visible = false;
  if (suppress) {
    // The caller ignores this void-like element renderer's result. Skipping
    // only the source-derived image ordinal removes the baked Xbox sprite
    // without touching the prompt's glyph elements, tag table, material bank,
    // or interaction behavior.
    visible =
        fable2::legacyprompt::PlausiblyVisibleWithoutRender(element, base);
    fable2::promptoverlay::NoteLegacyTitleSpriteSuppressed();
  } else {
    __imp__UITextPrompt_RenderElement(ctx, base);
    visible = fable2::legacyprompt::WasVisibleAfterRender(element, base);
  }

  // Count every prompt element even when its baked image draw is suppressed.
  // The enclosing frame classifier must keep seeing the original layout shape
  // so a relayout/device switch immediately disables suppression.
  fable2::promptoverlay::NoteLegacyTitleElement(visible);
}
