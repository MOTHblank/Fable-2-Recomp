// Hand-tuned diagnostic wrapper for ProcessAndProcessAndProcess509_832C13A8.
//
// This function is the only 0x832BB018 callee currently described as doing
// complex vector work in the 0x832BB018 input cluster. Fable2RE's independent
// function database reports a direct memory offset only from r3 (+0), while
// the older manifest summary guessed a three-field r4 structure. Neither is
// treated as confirmed semantics here: the wrapper observes both plausible
// argument pointers while always running the original __imp__ body exactly once.
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

  // Independent Fable2RE metadata says this function directly dereferences
  // r3+0. Keep a secondary r4 observation because the older manifest summary
  // inferred structure fields there; logging both resolves the disagreement.
  const bool observe_r3 =
      trace && fable2::mouselook::is_plausible_guest_ptr(arg3);
  const bool observe_r4 =
      trace && fable2::mouselook::is_plausible_guest_ptr(arg4);
  uint32_t r3_before[3] = {};
  uint32_t r4_before[3] = {};
  if (trace) {
    last_sample = sample.sequence;
    ++logged;
  }
  if (observe_r3) {
    r3_before[0] = REX_LOAD_U32(arg3 + 0);
    r3_before[1] = REX_LOAD_U32(arg3 + 4);
    r3_before[2] = REX_LOAD_U32(arg3 + 8);
  }
  if (observe_r4) {
    r4_before[0] = REX_LOAD_U32(arg4 + 0);
    r4_before[1] = REX_LOAD_U32(arg4 + 4);
    r4_before[2] = REX_LOAD_U32(arg4 + 8);
  }

  __imp__ProcessAndProcessAndProcess509_832C13A8(ctx, base);

  if (trace) {
    uint32_t r3_after[3] = {};
    uint32_t r4_after[3] = {};
    if (observe_r3) {
      r3_after[0] = REX_LOAD_U32(arg3 + 0);
      r3_after[1] = REX_LOAD_U32(arg3 + 4);
      r3_after[2] = REX_LOAD_U32(arg3 + 8);
    }
    if (observe_r4) {
      r4_after[0] = REX_LOAD_U32(arg4 + 0);
      r4_after[1] = REX_LOAD_U32(arg4 + 4);
      r4_after[2] = REX_LOAD_U32(arg4 + 8);
    }
    REXSYS_INFO(
        "[mouse-normalize-candidate] seq={} mouse={:.3f},{:.3f} "
        "rx={}->{} ry={}->{} "
        "r=[{:08X},{:08X},{:08X},{:08X}]->"
        "[{:08X},{:08X},{:08X},{:08X}] "
        "r3mem=[{:08X},{:08X},{:08X}]->[{:08X},{:08X},{:08X}] "
        "r3f=[{:.5f},{:.5f},{:.5f}]->[{:.5f},{:.5f},{:.5f}] "
        "r4mem=[{:08X},{:08X},{:08X}]->[{:08X},{:08X},{:08X}] "
        "r4f=[{:.5f},{:.5f},{:.5f}]->[{:.5f},{:.5f},{:.5f}] "
        "fpr=[{:.5f},{:.5f},{:.5f},{:.5f}]->"
        "[{:.5f},{:.5f},{:.5f},{:.5f}] ptrs={},{}",
        sample.sequence, sample.mouse_dx, sample.mouse_dy,
        sample.native_rx, sample.injected_rx,
        sample.native_ry, sample.injected_ry,
        arg3, arg4, arg5, arg6,
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        r3_before[0], r3_before[1], r3_before[2],
        r3_after[0], r3_after[1], r3_after[2],
        BitsAsFloat(r3_before[0]), BitsAsFloat(r3_before[1]),
        BitsAsFloat(r3_before[2]), BitsAsFloat(r3_after[0]),
        BitsAsFloat(r3_after[1]), BitsAsFloat(r3_after[2]),
        r4_before[0], r4_before[1], r4_before[2],
        r4_after[0], r4_after[1], r4_after[2],
        BitsAsFloat(r4_before[0]), BitsAsFloat(r4_before[1]),
        BitsAsFloat(r4_before[2]), BitsAsFloat(r4_after[0]),
        BitsAsFloat(r4_after[1]), BitsAsFloat(r4_after[2]),
        before_f1, before_f2, before_f3, before_f4,
        ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64,
        observe_r3 ? 1 : 0, observe_r4 ? 1 : 0);
  }
}
