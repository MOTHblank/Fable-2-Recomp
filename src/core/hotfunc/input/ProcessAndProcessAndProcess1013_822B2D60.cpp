// Hand-tuned override: guest-user-0 XInput boundary for direct mouse camera input.
//
// The original 11-line wrapper is still executed through its __imp__ symbol.
// After it fills the caller's final merged X_INPUT_STATE, this override adds
// raw mouse displacement to user 0's right stick only. That keeps physical
// controller input additive, avoids contaminating guest users 1-3, and feeds
// the exact state every downstream native input/camera consumer already uses.
#include "fable_2_pch.h"
#include "mouse_look.h"

#ifdef FABLE2_REMOTE_CONTROL
extern "C" void Fable2StateProbeObservePad(uint32_t state_ptr);
#endif

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/input/mnk/mnk_input_driver.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

REXCVAR_DECLARE(bool, mouse_direct_look);
REXCVAR_DECLARE(bool, mouse_look);
REXCVAR_DECLARE(int32_t, mouse_look_scale);
REXCVAR_DECLARE(int32_t, mouse_look_scale_y);
REXCVAR_DECLARE(bool, mouse_invert_y);
REXCVAR_DECLARE(int32_t, mouse_deadzone_compensation);

extern "C" void __imp__ProcessAndProcessAndProcess1013_822B2D60(
    PPCContext& ctx, uint8_t* base);

namespace {

constexpr uint32_t kGamepadOffset =
    offsetof(rex::input::X_INPUT_STATE, gamepad);
constexpr uint32_t kButtonsOffset =
    kGamepadOffset + offsetof(rex::input::X_INPUT_GAMEPAD, buttons);
constexpr uint32_t kLeftTriggerOffset =
    kGamepadOffset + offsetof(rex::input::X_INPUT_GAMEPAD, left_trigger);
constexpr uint32_t kThumbRxOffset =
    kGamepadOffset + offsetof(rex::input::X_INPUT_GAMEPAD, thumb_rx);
constexpr uint32_t kThumbRyOffset =
    kGamepadOffset + offsetof(rex::input::X_INPUT_GAMEPAD, thumb_ry);

static_assert(kButtonsOffset == 4);
static_assert(kLeftTriggerOffset == 6);
static_assert(kThumbRxOffset == 12);
static_assert(kThumbRyOffset == 14);

uint16_t AddMouseAxis(uint16_t raw, float pixels, int32_t units_per_pixel,
                      int32_t deadzone_compensation) {
  const int32_t native_axis =
      static_cast<int32_t>(static_cast<int16_t>(raw));
  const double scaled = std::clamp(
      double(pixels) * double(units_per_pixel), -32767.0, 32767.0);
  int32_t mouse_axis = static_cast<int32_t>(std::lround(scaled));

  // Match ReXGlue's Native-stick mouse mapping: compensation affects only the
  // mouse component, not the physical controller. It remaps non-zero motion
  // from [1..32767] into [deadzone+1..32767], preserving fine relative motion
  // after the game's stick deadzone without adding delayed/flick buffering.
  const int32_t deadzone = std::clamp(deadzone_compensation, 0, 12000);
  if (mouse_axis != 0 && deadzone > 0) {
    const int32_t sign = mouse_axis < 0 ? -1 : 1;
    const int32_t magnitude = std::abs(mouse_axis);
    const int32_t compensated = static_cast<int32_t>(std::lround(
        deadzone + double(magnitude) * double(32767 - deadzone) / 32767.0));
    mouse_axis = sign * std::clamp(compensated, 1, 32767);
  }

  const int32_t combined =
      std::clamp(native_axis + mouse_axis, -32768, 32767);
  return static_cast<uint16_t>(static_cast<int16_t>(combined));
}

}  // namespace

extern "C" void ProcessAndProcessAndProcess1013_822B2D60(
    PPCContext& ctx, uint8_t* base) {
  // The existing state-probe override established this ABI at runtime:
  // r3 = guest user index, r4 = X_INPUT_STATE* output buffer.
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t state_ptr = ctx.r4.u32;

  __imp__ProcessAndProcessAndProcess1013_822B2D60(ctx, base);

#ifdef FABLE2_REMOTE_CONTROL
  // Preserve the existing debug classifier's final-merged-state observation
  // without including its strong-override header in this translation unit.
  if (state_ptr != 0) {
    Fable2StateProbeObservePad(state_ptr);
  }
#endif

  // XamInputGetState-style success is 0. Direct mouse belongs to guest user 0;
  // all other users must receive the original merged state byte-for-byte.
  if (ctx.r3.u32 != 0 || user_index != 0 || state_ptr == 0 ||
      !REXCVAR_GET(mouse_direct_look) || !REXCVAR_GET(mouse_look)) {
    return;
  }

  // Offsets are derived from the pinned SDK structs above and asserted against
  // the Xbox X_INPUT_STATE ABI, rather than duplicated as unchecked literals.
  const uint16_t buttons = REX_LOAD_U16(state_ptr + kButtonsOffset);
  const uint8_t left_trigger = REX_LOAD_U8(state_ptr + kLeftTriggerOffset);
  constexpr uint8_t kTriggerThreshold = 30;
  const uint16_t special_camera_buttons =
      rex::input::X_INPUT_GAMEPAD_RIGHT_THUMB |
      rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER;

  if (left_trigger >= kTriggerThreshold ||
      (buttons & special_camera_buttons) != 0) {
    // LT = Look-At / Target Lock, LB = first-person, R3 = View Hero.
    // Those modes keep the SDK's ordinary virtual-stick mapping while active.
    rex::input::mnk::SetDirectMouseEligible(false);
    return;
  }

  // Direct injection is safe for ordinary gameplay camera states because this
  // is still only a native right-stick value; collision, authored framing and
  // camera constraints remain downstream in the game's own controller.
  rex::input::mnk::SetDirectMouseEligible(true);

  float mouse_dx = 0.0f;
  float mouse_dy = 0.0f;
  // The game may poll user 0 multiple times before camera update. Present the
  // same raw displacement on every poll; the camera-frame boundary drains it.
  fable2::mouselook::peek_delta(mouse_dx, mouse_dy);
  if (!std::isfinite(mouse_dx) || !std::isfinite(mouse_dy)) {
    fable2::mouselook::request_reset();
    return;
  }

  const int32_t scale_x = REXCVAR_GET(mouse_look_scale);
  const int32_t scale_y = REXCVAR_GET(mouse_look_scale_y);
  const int32_t deadzone = REXCVAR_GET(mouse_deadzone_compensation);
  if (scale_x < 1 || scale_x > 4096 || scale_y < 1 || scale_y > 4096 ||
      deadzone < 0 || deadzone > 12000) {
    return;
  }

  if (mouse_dx == 0.0f && mouse_dy == 0.0f) {
    return;
  }

  const uint16_t native_rx = REX_LOAD_U16(state_ptr + kThumbRxOffset);
  const uint16_t native_ry = REX_LOAD_U16(state_ptr + kThumbRyOffset);
  const int32_t y_scale =
      REXCVAR_GET(mouse_invert_y) ? scale_y : -scale_y;

  const uint16_t injected_rx =
      AddMouseAxis(native_rx, mouse_dx, scale_x, deadzone);
  const uint16_t injected_ry =
      AddMouseAxis(native_ry, mouse_dy, y_scale, deadzone);
  REX_STORE_U16(state_ptr + kThumbRxOffset, injected_rx);
  REX_STORE_U16(state_ptr + kThumbRyOffset, injected_ry);
  fable2::mouselook::record_native_input_sample(
      static_cast<int16_t>(native_rx), static_cast<int16_t>(native_ry),
      static_cast<int16_t>(injected_rx), static_cast<int16_t>(injected_ry),
      mouse_dx, mouse_dy);
  fable2::mouselook::note_native_injection();
}
