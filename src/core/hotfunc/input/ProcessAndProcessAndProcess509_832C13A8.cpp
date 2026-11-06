// Hand-tuned diagnostic wrapper for ProcessAndProcessAndProcess509_832C13A8.
//
// This function is the only 0x832BB018 callee currently described as doing
// complex vector work in the 0x832BB018 input cluster. Fable2RE's independent
// function database reports a direct memory offset only from r3 (+0). The
// older manifest summary guessed a larger r4 structure, but that is not strong
// enough evidence to dereference here. The wrapper therefore observes only the
// proven r3+0 word plus registers/FPRs, and always runs __imp__ exactly once.
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
  const uint32_t arg4 = ctx.r4.u32;
  const uint32_t arg5 = ctx.r5.u32;
  const uint32_t arg6 = ctx.r6.u32;
  const double before_f1 = ctx.f1.f64;
  const double before_f2 = ctx.f2.f64;
  const double before_f3 = ctx.f3.f64;
  const double before_f4 = ctx.f4.f64;

  // Fable2RE's independent static database records exactly one direct memory
  // offset for this function: r3+0. Do not widen the probe beyond that.
  const bool observe_r3 =
      trace && fable2::mouselook::is_plausible_guest_ptr(arg3);
  uint32_t r3_before = 0;
  if (trace) {
    last_sample = sample.sequence;
    ++logged;
  }
  if (observe_r3) {
    r3_before = REX_LOAD_U32(arg3);
  }

  __imp__ProcessAndProcessAndProcess509_832C13A8(ctx, base);

  if (trace) {
    const uint32_t r3_after = observe_r3 ? REX_LOAD_U32(arg3) : 0;
    REXSYS_INFO(
        "[mouse-normalize-candidate] seq={} mouse={:.3f},{:.3f} "
        "rx={}->{} ry={}->{} "
        "r=[{:08X},{:08X},{:08X},{:08X}]->"
        "[{:08X},{:08X},{:08X},{:08X}] "
        "r3mem={:08X}->{:08X} r3f={:.6f}->{:.6f} "
        "fpr=[{:.6f},{:.6f},{:.6f},{:.6f}]->"
        "[{:.6f},{:.6f},{:.6f},{:.6f}] r3ptr={}",
        sample.sequence, sample.mouse_dx, sample.mouse_dy,
        sample.native_rx, sample.injected_rx,
        sample.native_ry, sample.injected_ry,
        arg3, arg4, arg5, arg6,
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        r3_before, r3_after,
        BitsAsFloat(r3_before), BitsAsFloat(r3_after),
        before_f1, before_f2, before_f3, before_f4,
        ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64,
        observe_r3 ? 1 : 0);
  }
}
