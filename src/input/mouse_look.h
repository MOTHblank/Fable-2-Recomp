// fable_2 - Raw mouse transport and guest-memory camera helpers.
//
// Native mode synthesizes a right stick through the ReXGlue input driver.
// Hybrid Direct mode accumulates SDL3 relative deltas on the UI thread. The
// guest-user-0 XamInputGetState wrapper snapshots them into the final merged
// right-stick state; the later camera-update boundary drains the frame delta.
#pragma once

#include <cmath>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/system/xmemory.h>  // rex::memory::Memory (guest address -> host pointer)

namespace fable2::mouselook {

// Accumulated raw cursor displacement (host pixels) since the last drain.
// X = right is positive, Y = down is positive (screen coordinates); the hook
// applies the sign/scale from fable2_config.
inline std::atomic<float>& dx_acc() {
  static std::atomic<float> v{0.0f};
  return v;
}
inline std::atomic<float>& dy_acc() {
  static std::atomic<float> v{0.0f};
  return v;
}

// Diagnostic counters: raw UI motion can be distinguished from a hook that
// fires but never receives input. 64-bit atomics avoid races with SDL's UI thread.
inline std::atomic<uint64_t>& raw_motion_events() {
  static std::atomic<uint64_t> events{0};
  return events;
}
inline std::atomic<uint64_t>& capture_resets() {
  static std::atomic<uint64_t> resets{0};
  return resets;
}
inline std::atomic<uint64_t>& native_injections() {
  static std::atomic<uint64_t> injections{0};
  return injections;
}
inline void note_native_injection() {
  native_injections().fetch_add(1, std::memory_order_relaxed);
}

// Latest successful guest-user-0 Direct injection. Input and camera processing
// are on the guest thread, so relaxed fields plus a release/acquire sequence
// number are enough for diagnostic correlation without locks or allocations.
struct NativeInputSample {
  uint64_t sequence = 0;
  int32_t native_rx = 0;
  int32_t native_ry = 0;
  int32_t injected_rx = 0;
  int32_t injected_ry = 0;
  float mouse_dx = 0.0f;
  float mouse_dy = 0.0f;
};
inline std::atomic<uint64_t>& native_input_sequence() {
  static std::atomic<uint64_t> v{0};
  return v;
}
inline std::atomic<int32_t>& last_native_rx() {
  static std::atomic<int32_t> v{0};
  return v;
}
inline std::atomic<int32_t>& last_native_ry() {
  static std::atomic<int32_t> v{0};
  return v;
}
inline std::atomic<int32_t>& last_injected_rx() {
  static std::atomic<int32_t> v{0};
  return v;
}
inline std::atomic<int32_t>& last_injected_ry() {
  static std::atomic<int32_t> v{0};
  return v;
}
inline std::atomic<float>& last_injected_mouse_dx() {
  static std::atomic<float> v{0.0f};
  return v;
}
inline std::atomic<float>& last_injected_mouse_dy() {
  static std::atomic<float> v{0.0f};
  return v;
}
inline void record_native_input_sample(int32_t native_rx, int32_t native_ry,
                                       int32_t injected_rx, int32_t injected_ry,
                                       float mouse_dx, float mouse_dy) {
  last_native_rx().store(native_rx, std::memory_order_relaxed);
  last_native_ry().store(native_ry, std::memory_order_relaxed);
  last_injected_rx().store(injected_rx, std::memory_order_relaxed);
  last_injected_ry().store(injected_ry, std::memory_order_relaxed);
  last_injected_mouse_dx().store(mouse_dx, std::memory_order_relaxed);
  last_injected_mouse_dy().store(mouse_dy, std::memory_order_relaxed);
  native_input_sequence().fetch_add(1, std::memory_order_release);
}
inline NativeInputSample latest_native_input_sample() {
  NativeInputSample s;
  s.sequence = native_input_sequence().load(std::memory_order_acquire);
  s.native_rx = last_native_rx().load(std::memory_order_relaxed);
  s.native_ry = last_native_ry().load(std::memory_order_relaxed);
  s.injected_rx = last_injected_rx().load(std::memory_order_relaxed);
  s.injected_ry = last_injected_ry().load(std::memory_order_relaxed);
  s.mouse_dx = last_injected_mouse_dx().load(std::memory_order_relaxed);
  s.mouse_dy = last_injected_mouse_dy().load(std::memory_order_relaxed);
  return s;
}

// Host input driver: add this poll's cursor displacement.
inline void add_delta(float dx, float dy) {
  if (std::isfinite(dx) && std::isfinite(dy)) {
    dx_acc().fetch_add(dx, std::memory_order_relaxed);
    dy_acc().fetch_add(dy, std::memory_order_relaxed);
    if (dx != 0.0f || dy != 0.0f)
      raw_motion_events().fetch_add(1, std::memory_order_relaxed);
  }
}

// Input processing may run the verified XINPUT consumer more than once per
// guest frame. Every invocation must see the same pending displacement; the
// camera-update boundary drains it after input processing has finished.
inline void peek_delta(float& out_dx, float& out_dy) {
  out_dx = dx_acc().load(std::memory_order_relaxed);
  out_dy = dy_acc().load(std::memory_order_relaxed);
}

// Guest camera/frame boundary: consume and reset the accumulated displacement.
inline void take_delta(float& out_dx, float& out_dy) {
  out_dx = dx_acc().exchange(0.0f, std::memory_order_relaxed);
  out_dy = dy_acc().exchange(0.0f, std::memory_order_relaxed);
}
inline void request_reset() {
  capture_resets().fetch_add(1, std::memory_order_relaxed);
  dx_acc().store(0.0f, std::memory_order_relaxed);
  dy_acc().store(0.0f, std::memory_order_relaxed);
}

// Guest memory, set once at image load (Fable2App::OnPostLoadXexImage) so the
// camera hook can translate the camera object's guest address to a host
// pointer. Null until set; the hook no-ops while it is null.
inline std::atomic<rex::memory::Memory*>& memory_ref() {
  static std::atomic<rex::memory::Memory*> m{nullptr};
  return m;
}
inline void set_memory(rex::memory::Memory* mem) { memory_ref().store(mem, std::memory_order_release); }

// --- Big-endian guest float access (matches REX_LOAD_U32 / REX_STORE_U32) ----
inline float read_guest_float(rex::memory::Memory* mem, uint32_t guest_addr) {
  const uint32_t* p = mem->TranslateVirtual<const uint32_t*>(guest_addr);
  uint32_t bits = __builtin_bswap32(*p);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
inline uint32_t read_guest_u32(rex::memory::Memory* mem, uint32_t guest_addr) {
  const uint32_t* p = mem->TranslateVirtual<const uint32_t*>(guest_addr);
  return __builtin_bswap32(*p);
}
// True if guest_addr points into a plausibly-valid guest region. Two ranges
// matter: the recompiled image (code + data + globals) is mapped at 0x82xxxxxx,
// while live heap objects (the CECCamera, its controller, etc.) sit lower at
// 0x42xxxxxx. Accepting both lets multi-level chains hop between heap objects
// and image globals. Excludes 0/1 sentinels and tiny garbage.
inline bool is_plausible_guest_ptr(uint32_t a) {
  return (a >= 0x40000000u && a < 0x48000000u) ||  // live heap objects
         (a >= 0x82000000u && a < 0x84000000u);      // recompiled image
}
// Bounds-checked u32 read: returns ok=false (and leaves *out=0) if the address
// is not a plausible guest pointer. Never dereferences an out-of-range address.
inline bool try_read_guest_u32(rex::memory::Memory* mem, uint32_t guest_addr,
                               uint32_t* out) {
  if (!is_plausible_guest_ptr(guest_addr)) { *out = 0; return false; }
  *out = read_guest_u32(mem, guest_addr);
  return true;
}

// --- Big-endian guest 3-vector access ---------------------------------------
// The active camera's world-space EYE position is the 3-float at camera+144
// (the 4th float at +156 is always 0). When the game's own right-stick look
// orbits the camera (hero stationary), this vector sweeps a circle of radius
// ~3.1 around the hero -- it is the orbiting eye, not the look-at/hero point.
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
inline Vec3 read_guest_vec3(rex::memory::Memory* mem, uint32_t guest_addr) {
  return Vec3{read_guest_float(mem, guest_addr + 0),
              read_guest_float(mem, guest_addr + 4),
              read_guest_float(mem, guest_addr + 8)};
}
inline float vec3_len(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
}  // namespace fable2::mouselook
