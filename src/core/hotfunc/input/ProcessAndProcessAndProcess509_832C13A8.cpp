// Hand-tuned diagnostic wrapper for ProcessAndProcessAndProcess509_832C13A8.
//
// This function is the only 0x832BB018 callee currently described as doing
// complex vector work on a three-field structure. It is a candidate for the
// raw-stick -> normalized-control transition, but that is not assumed here.
// The original __imp__ body always runs exactly once. With mouse-camera
// diagnostics enabled, this wrapper records the three u32/float fields before
// and after one call for each new guest-user-0 Direct input sample.
#include "fable_2_pch.h"
#include "fable2_config.h"
#include "mouse_look.h"

#include <cstdint>
#include <cstring>

extern "C" void __imp__ProcessAndProcessAndProcess509_832C13A8(
    PPCContext& ctx, uint8_t* base);

namespace {

float BitsAsFloat(uint32_t bits) {
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace

extern "C" void ProcessAndProcessAndProcess509_832C13A8(
    PPCContext& ctx, uint8_t* base) {
  const auto sample = fable2::mouselook::latest_native_input_sample();
  static uint64_t last_sample = 0;
  static uint32_t logged = 0;

  const bool trace =
      fable2::config::Get().mouse_look_camera_diag &&
      sample.sequence != 0 &&
      sample.sequence != last_sample &&
      logged < 400;

  const uint32_t arg3 = ctx.r3.u32;
  const uint32_t object = ctx.r4.u32;
  const uint32_t arg5 = ctx.r5.u32;
  const uint32_t arg6 = ctx.r6.u32;
  const double before_f1 = ctx.f1.f64;
  const double before_f2 = ctx.f2.f64;
  const double before_f3 = ctx.f3.f64;
  const double before_f4 = ctx.f4.f64;
  uint32_t before0 = 0;
  uint32_t before4 = 0;
  uint32_t before8 = 0;
  const bool observe_fields =
      trace && fable2::mouselook::is_plausible_guest_ptr(object);
  if (trace) {
    last_sample = sample.sequence;
    ++logged;
  }
  if (observe_fields) {
    before0 = REX_LOAD_U32(object + 0);
    before4 = REX_LOAD_U32(object + 4);
    before8 = REX_LOAD_U32(object + 8);
  }

  __imp__ProcessAndProcessAndProcess509_832C13A8(ctx, base);

  if (trace) {
    const uint32_t after0 = observe_fields ? REX_LOAD_U32(object + 0) : 0;
    const uint32_t after4 = observe_fields ? REX_LOAD_U32(object + 4) : 0;
    const uint32_t after8 = observe_fields ? REX_LOAD_U32(object + 8) : 0;
    REXSYS_INFO(
        "[mouse-normalize-candidate] seq={} mouse={:.3f},{:.3f} "
        "rx={}->{} ry={}->{} "
        "r=[{:08X},{:08X},{:08X},{:08X}]->"
        "[{:08X},{:08X},{:08X},{:08X}] "
        "u32=[{:08X},{:08X},{:08X}]->[{:08X},{:08X},{:08X}] "
        "as-f32=[{:.5f},{:.5f},{:.5f}]->[{:.5f},{:.5f},{:.5f}] "
        "fpr=[{:.5f},{:.5f},{:.5f},{:.5f}]->"
        "[{:.5f},{:.5f},{:.5f},{:.5f}] fields={}",
        sample.sequence, sample.mouse_dx, sample.mouse_dy,
        sample.native_rx, sample.injected_rx,
        sample.native_ry, sample.injected_ry,
        arg3, object, arg5, arg6,
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        before0, before4, before8, after0, after4, after8,
        BitsAsFloat(before0), BitsAsFloat(before4), BitsAsFloat(before8),
        BitsAsFloat(after0), BitsAsFloat(after4), BitsAsFloat(after8),
        before_f1, before_f2, before_f3, before_f4,
        ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64,
        observe_fields ? 1 : 0);
  }
}
