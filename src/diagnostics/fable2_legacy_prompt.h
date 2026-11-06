// Last-resort keyboard prompt bridge for Fable II's legacy UITextPrompt path.
//
// The ordinary adaptive-prompt pipeline rewrites writable <a_img> source
// markup before layout. If that source allocation has already disappeared,
// UITextPrompt_RenderElement receives only baked element/drawable data.
//
// The complete element renderer always runs so visibility, animation and
// bookkeeping stay native. Once the localized source scanner identifies the
// exact baked <x_img> element ordinal and a prior frame confirms the stale
// baked layout, keyboard mode suppresses only that element's final drawable
// vtable call. All text glyphs and unknown layouts remain stock.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include <rex/cvar.h>
#include <rex/ppc/context.h>

#include "fable2_button_prompts.h"
#include "fable2_prompt_overlay.h"

extern "C" void __imp__UITextPrompt_Render(PPCContext& ctx,
                                            uint8_t* base);
extern "C" void __imp__UITextPrompt_RenderElement(PPCContext& ctx,
                                                   uint8_t* base);
extern "C" void __imp__UITextPrompt_DrawableRender(PPCContext& ctx,
                                                    uint8_t* base);

namespace fable2::legacyprompt {

inline thread_local uint32_t g_geometry_capture_drawable = 0;
inline thread_local bool g_suppress_capture_drawable = false;

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

inline void ArmGeometryCapture(uint32_t ordinal, uint32_t element,
                               bool suppress_draw, uint8_t* base) {
  g_geometry_capture_drawable = 0;
  g_suppress_capture_drawable = false;
  if (!base || !PlausibleElement(element) ||
      ordinal != fable2::buttonprompts::TitleExpectedIconElementOrdinal()) {
    return;
  }
  const uint32_t drawable = REX_LOAD_U32(element + 0x7C);
  if (drawable >= 0x10000u && drawable < 0xE0000000u &&
      (drawable & 3u) == 0) {
    g_geometry_capture_drawable = drawable;
    g_suppress_capture_drawable = suppress_draw;
  }
}

inline void DisarmGeometryCapture() {
  g_geometry_capture_drawable = 0;
  g_suppress_capture_drawable = false;
}

inline float GuestWordAsFloat(uint32_t word) {
  float value = 0.0f;
  static_assert(sizeof(value) == sizeof(word));
  std::memcpy(&value, &word, sizeof(value));
  return value;
}

inline bool ExtractPreparedCanvasPosition(uint32_t scratch, uint8_t* base,
                                          float& out_x, float& out_y) {
  if (!base || scratch < 0x10000u || scratch >= 0xE0000000u ||
      (scratch & 3u) != 0) {
    return false;
  }

  // The leaf receives a caller-prepared stack object in r3. Inspect only a
  // bounded 96-byte prefix and accept coordinates near the already verified
  // title position. This is intentionally an inference filter, not a generic
  // structure parser: if the draw scratch uses another representation the
  // fixed authored position remains active.
  constexpr uint32_t kWords = 24;
  constexpr float kReferenceX = 640.0f;
  constexpr float kReferenceY = 560.0f;
  constexpr float kMaxDx = 160.0f;
  constexpr float kMaxDy = 120.0f;

  float best_score = std::numeric_limits<float>::infinity();
  float best_x = 0.0f;
  float best_y = 0.0f;

  const auto consider = [&](float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y) ||
        x < kReferenceX - kMaxDx || x > kReferenceX + kMaxDx ||
        y < kReferenceY - kMaxDy || y > kReferenceY + kMaxDy) {
      return;
    }
    const float dx = x - kReferenceX;
    const float dy = y - kReferenceY;
    const float score = dx * dx + dy * dy;
    if (score < best_score) {
      best_score = score;
      best_x = x;
      best_y = y;
    }
  };

  for (uint32_t i = 0; i + 1 < kWords; ++i) {
    const uint32_t a = REX_LOAD_U32(scratch + i * 4);
    const uint32_t b = REX_LOAD_U32(scratch + (i + 1) * 4);

    consider(GuestWordAsFloat(a), GuestWordAsFloat(b));

    // Some UI helpers retain canvas coordinates as integer words until the
    // final draw. Recognize that representation too, under the same strict
    // title-local bounds.
    if (a <= 1280u && b <= 720u) {
      consider(static_cast<float>(a), static_cast<float>(b));
    }
  }

  if (!std::isfinite(best_score)) return false;
  out_x = best_x;
  out_y = best_y;
  return true;
}

}  // namespace fable2::legacyprompt

extern "C" void UITextPrompt_Render(PPCContext& ctx, uint8_t* base) {
  fable2::promptoverlay::BeginLegacyTitleFrame(ctx.r3.u32);
  __imp__UITextPrompt_Render(ctx, base);
  fable2::promptoverlay::EndLegacyTitleFrame();
}

extern "C" void UITextPrompt_RenderElement(PPCContext& ctx, uint8_t* base) {
  const uint32_t element = ctx.r3.u32;
  const uint32_t ordinal =
      fable2::promptoverlay::CurrentLegacyTitleElementOrdinal();
  const bool suppress_draw =
      fable2::promptoverlay::ShouldSuppressLegacyTitleElement(ordinal, element);

  // Always run the complete stock element renderer. It owns visibility,
  // animation and per-element bookkeeping that should continue even when the
  // baked controller sprite itself is replaced. The source-derived image
  // element arms the named drawable leaf below; only that final draw call may
  // be skipped.
  fable2::legacyprompt::ArmGeometryCapture(
      ordinal, element, suppress_draw, base);
  __imp__UITextPrompt_RenderElement(ctx, base);
  fable2::legacyprompt::DisarmGeometryCapture();

  fable2::promptoverlay::NoteLegacyTitleElement(
      element, fable2::legacyprompt::WasVisibleAfterRender(element, base));
}

extern "C" void UITextPrompt_DrawableRender(PPCContext& ctx, uint8_t* base) {
  const bool matched =
      fable2::legacyprompt::g_geometry_capture_drawable != 0 &&
      ctx.r4.u32 == fable2::legacyprompt::g_geometry_capture_drawable;
  if (matched) {
    float x = 0.0f;
    float y = 0.0f;
    if (fable2::legacyprompt::ExtractPreparedCanvasPosition(
            ctx.r3.u32, base, x, y)) {
      fable2::promptoverlay::NoteLegacyTitleDrawPosition(x, y);
    }

    if (fable2::legacyprompt::g_suppress_capture_drawable) {
      // Preserve all enclosing UITextPrompt_RenderElement bookkeeping and
      // suppress only vtable slot 8 for the exact confirmed title sprite.
      fable2::promptoverlay::NoteLegacyTitleSpriteSuppressed();
      return;
    }
  }

  __imp__UITextPrompt_DrawableRender(ctx, base);
}
