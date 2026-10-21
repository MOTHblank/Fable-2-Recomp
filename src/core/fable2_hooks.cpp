// fable2_hooks.cpp - mid-asm hook functions for the [[entrypoint.midasm_hook]]
// entries in fable_2_manifest.toml (see docs/patches.md).
//
// Codegen emits an extern prototype for each hook into the generated code
// (e.g. `extern void fable2_hook_website_g1(PPCRegister& r9);`) and calls it at
// the configured instruction address, passing the named registers BY
// REFERENCE, so a hook can read and/or rewrite them. Define each hook with
// plain C++ linkage (NOT extern "C") and exactly once, matching the emitted
// prototype.
//
// Every patch hook is gated on a toggle in fable2_config.toml ([patches]),
// consulted on each call, so a patch can be A/B'd without a rebuild.

#include <rex/ppc/context.h>  // PPCRegister (union with u64/s64/u32/... views)
#include <rex/logging/macros.h>

#include <string>
#include <algorithm>
#include <cmath>
#include <rex/cvar.h>

REXCVAR_DECLARE(bool, mouse_direct_look);
REXCVAR_DECLARE(bool, mouse_look);
REXCVAR_DECLARE(int32_t, mouse_look_scale);
REXCVAR_DECLARE(int32_t, mouse_look_scale_y);
REXCVAR_DECLARE(bool, mouse_invert_y);

#include <fmt/format.h>

#include "fable2_config.h"
#include "mouse_look.h"  // guest camera memory read helpers for diagnostics

// ---------------------------------------------------------------------------
// Guild chest unlock (fable2.com website items + Collectors Edition content).
//
// An unregistered player who clicks the Guild chest sees "Go to www.fable2.com
// for information on how to access the gold and items your heroic ancestors
// left behind." The chest contents are decided by two getter functions that
// run at save load:
//   - GuildChest_GetWebsiteItem_8256E368 (website items)
//   - GuildChest_GetCEContent_824B3528    (CE content)
//
// Each getter is gated by two "registered" flag bits on the chest object, and
// only when both pass does it find the item and call a per-object GRANT vtable
// method, returning THAT method's result. For an unregistered player the gates
// fail (and even where they pass, the grant method returns 0), so the getter
// reports "not available" and the chest stays locked. We force BOTH:
//   1. the two gate bit-extract results to 1 (g1 / g1b hooks below), and
//   2. the grant method's return to 1 (grantnew / grantavail hooks below).
//
// The getters run at save load, so this populates the chest inventory on load;
// the chest then opens normally and shows the website + CE items.
//
// Toggle: [patches] unlock_website / unlock_ce in fable2_config.toml
// (both default true).
// ---------------------------------------------------------------------------

// Unlock Website Items. GATE 1 (bit6 of *(r4+0x90)): the `rlwinm r9, r10, 0,
// 0x19, 0x19` at 0x8256E384 extracts bit 6 into r9 (0x40 or 0). Force r9 = 1
// so the gate always passes.
void fable2_hook_website_g1(PPCRegister& r9) {
  if (fable2::config::Get().unlock_website) {
    r9.u64 = 1;
  }
}

// Unlock Website Items. GATE 1b (bit0 of *(r4+0x40)): the `clrlwi r8, r9,
// 0x1f` at 0x8256E3AC extracts bit 0 into r8 (1 or 0). Force r8 = 1 so the
// gate always passes.
void fable2_hook_website_g1b(PPCRegister& r8) {
  if (fable2::config::Get().unlock_website) {
    r8.u64 = 1;
  }
}

// Unlock Website Items. Grant method sub_8256D940 (the website-chest
// vtable[1] "is this item new?" check): its final instruction is `xori r3,
// r9, 1` at 0x8256D9E4. It returns 0 when the item hash is already in the
// grant list; force r3 = 1 so the getter reports the item as granted.
void fable2_hook_website_grantnew(PPCRegister& r3) {
  if (fable2::config::Get().unlock_website) {
    r3.u32 = 1;
  }
}

// Unlock Collectors Edition Content. GATE 1 (bit6 of *(r4+0x90)): the
// `rlwinm r10, r11, 0, 0x19, 0x19` at 0x824B3540 extracts bit 6 into r10
// (0x40 or 0). Force r10 = 1 so the gate always passes.
void fable2_hook_ce_g1(PPCRegister& r10) {
  if (fable2::config::Get().unlock_ce) {
    r10.u64 = 1;
  }
}

// Unlock Collectors Edition Content. GATE 1b (a bit of *(r4+0x28)): the
// `rlwinm r9, r10, 7, 0x1f, 0x1f` at 0x824B3568 extracts the bit into r9
// (1 or 0). Force r9 = 1 so the gate always passes.
void fable2_hook_ce_g1b(PPCRegister& r9) {
  if (fable2::config::Get().unlock_ce) {
    r9.u64 = 1;
  }
}

// Unlock Collectors Edition Content. Grant method sub_824ACAE0 (the CE-chest
// grant vtable slot called from GetCEContent): its entire body is `lbz r3,
// 993(r3)` (returns the byte at item+0x3E1, the "CE content available" flag).
// Force r3 = 1 so the getter reports the CE content as available.
void fable2_hook_ce_grantavail(PPCRegister& r3) {
  if (fable2::config::Get().unlock_ce) {
    r3.u32 = 1;
  }
}

// Disable Motion Blur. Runs right after `lfs f12, 0xd4(r31)` at 0x822A49E8 in
// the camera update, where the camera's current full-screen motion blur amount
// (+0xD4) is loaded to be copied into the renderer's view settings (+0x7C).
// Forcing f12 to 0 means the renderer never applies the blur, while the camera
// object and any script reading Camera.GetBlur still see the game's value.
// The first non-zero request is logged once, so the log shows whether the game
// actually asked for motion blur during play.
void fable2_hook_disable_motion_blur(PPCRegister& f12) {
  static bool logged = false;
  const bool disable = fable2::config::Get().disable_motion_blur;
  if (!logged && f12.f64 != 0.0) {
    logged = true;
    REXSYS_INFO("[motion-blur] game requested full-screen motion blur {:.3f} ({})", f12.f64,
                disable ? "forced to 0" : "left on");
  }
  if (disable) {
    f12.f64 = 0.0;
  }
}

// Skip Intro Videos (just-harry's "Skip intro videos" patch, as a hook).
// sub_822F4958 builds the boot video queue (microsoft_logo.bik,
// lionhead_logo.bik, terminator) and loops queueing slots until
// CompareString_8229AD78 reports the terminator. This runs right after the
// first CompareString (bl at 0x822F49B8): r3 = 0 means "slot 0 is the
// terminator", so the loop is skipped and no intro video is queued. Harry's
// original NOPs the slot-0 construction instead; the hook leaves all three
// strings constructed and released normally. Runs once per boot.
void fable2_hook_skip_intro_videos(PPCRegister& r3) {
  if (fable2::config::Get().skip_intro_videos) {
    r3.u64 = 0;
  }
}

// High Tick Rate (Xenia patch by Guy), code half. Runs BEFORE the store at
// 0x8233AEB4 that writes the game's LF tick-rate double (0x83319510);
// returning true jumps past it, which is exactly the Xenia patch's NOP.
// Without this the game overwrote the patched value (15 Hz -> 30 Hz, written
// at load by src/core/fable2_patches.cpp) and the patch had no effect.
// Toggle: [patches] high_tick_rate.
bool fable2_hook_high_tick_rate_skip_store() {
  static const bool enabled = [] {
    const bool on = fable2::config::Get().high_tick_rate;
    if (on) {
      REXSYS_INFO("[tick-rate] LF tick forced to 30 Hz (high_tick_rate)");
    }
    return on;
  }();
  return enabled;
}

// Higher HF Tick Rate (Xenia patch by Ultra), code half. Runs BEFORE the store
// at 0x8233AE98 that writes the HF tick double (0x83319518, normally 30 Hz =
// twice the 15 Hz LF tick). Skipping it keeps the patched 60 Hz, so with
// high_tick_rate on the HF:LF ratio stays 2:1 (60:30) instead of collapsing
// to 1:1 (30:30), which made cloth physics misbehave.
// Requires high_tick_rate: on its own (HF 60 Hz with LF 15 Hz, 4:1) it would
// break the 2:1 ratio the other way, so it is ignored unless both are on.
// Toggle: [patches] higher_hf_tick_rate (needs [patches] high_tick_rate).
bool fable2_hook_high_hf_tick_rate_skip_store() {
  static const bool enabled = [] {
    const auto& cfg = fable2::config::Get();
    if (cfg.higher_hf_tick_rate && !cfg.high_tick_rate) {
      REXSYS_WARN("[tick-rate] higher_hf_tick_rate ignored: it requires high_tick_rate");
      return false;
    }
    if (cfg.higher_hf_tick_rate) {
      REXSYS_INFO("[tick-rate] HF tick forced to 60 Hz (higher_hf_tick_rate)");
    }
    return cfg.higher_hf_tick_rate;
  }();
  return enabled;
}

// Experimental two-axis orbit applied after the original camera update.
// The default native-stick path is unchanged; gamepad state is never modified.
static void ApplyDirectMouseOrbit(rex::memory::Memory* mem, uint32_t cam,
                                 int32_t sensitivity_x, int32_t sensitivity_y,
                                 bool invert_y) {
  using namespace fable2::mouselook;
  if (take_reset_request()) reset_cumulative();
  // Identified hero-follow camera flag; skip scripted/UI camera objects.
  if (read_guest_u32(mem, cam + 108) == 0) return;
  const Vec3 eye = read_guest_vec3(mem, cam + 144);
  const Quat native_q = read_guest_quat(mem, cam + 160);
  const float norm = native_q.x * native_q.x + native_q.y * native_q.y +
                     native_q.z * native_q.z + native_q.w * native_q.w;
  if (!std::isfinite(norm) || norm < 0.5f || norm > 1.5f ||
      !std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z))
    return;

  float dx = 0.0f, dy = 0.0f;
  take_delta(dx, dy);
  // 256 sensitivity -> 0.15 degrees per pixel; independent of poll frequency.
  constexpr float kRadPerPixel = 0.00261799388f;
  cum_yaw() = std::remainder(cum_yaw() +
      dx * kRadPerPixel * (float(sensitivity_x) / 256.0f), 6.283185307f);
  cum_pitch() = std::clamp(cum_pitch() +
      dy * kRadPerPixel * (float(sensitivity_y) / 256.0f) *
        (invert_y ? 1.0f : -1.0f), -1.15f, 1.15f);

  // Rebase each frame onto the native follow pose, avoiding pivot drift.
  const Vec3 up{0.0f, 0.0f, 1.0f};
  const Vec3 native_forward = vec3_normalize(
      quat_rotate_vec(native_q, Vec3{0.0f, 1.0f, 0.0f}));
  const Vec3 native_right = vec3_normalize(
      quat_rotate_vec(native_q, Vec3{1.0f, 0.0f, 0.0f}));
  if (vec3_len(native_forward) < 0.9f || vec3_len(native_right) < 0.9f) return;
  const Vec3 pitched = quat_rotate_vec(
      axis_quat_vec(native_right, cum_pitch()), native_forward);
  const Vec3 forward = vec3_normalize(
      quat_rotate_vec(axis_quat_vec(up, cum_yaw()), pitched));
  const Vec3 right = vec3_normalize(vec3_cross(forward, up));
  if (vec3_len(right) < 0.01f) return;
  const Vec3 camera_up = vec3_normalize(vec3_cross(right, forward));
  const Quat new_q = quat_from_basis(right, forward, camera_up);

  // Reverse-engineered default orbit radius; collision is not replicated.
  constexpr float radius = 3.1f;
  const Vec3 pivot{eye.x + native_forward.x * radius,
                   eye.y + native_forward.y * radius,
                   eye.z + native_forward.z * radius};
  write_guest_vec3(mem, cam + 144,
      Vec3{pivot.x - forward.x * radius,
           pivot.y - forward.y * radius,
           pivot.z - forward.z * radius});
  write_guest_quat(mem, cam + 160, new_q);
  write_guest_quat(mem, cam + 176, new_q);
}

// Camera post-update hook: native stick remains the default.
void fable2_hook_direct_mouse_look(PPCRegister& r31) {
  const auto& cfg = fable2::config::Get();
  rex::memory::Memory* mem =
      fable2::mouselook::memory_ref().load(std::memory_order_acquire);
  if (!mem) return;
  const uint32_t cam = r31.u32;
  if (cam == 0) return;
  if (REXCVAR_GET(mouse_look) && REXCVAR_GET(mouse_direct_look))
    ApplyDirectMouseOrbit(mem, cam, REXCVAR_GET(mouse_look_scale),
                          REXCVAR_GET(mouse_look_scale_y),
                          REXCVAR_GET(mouse_invert_y));

  // Diagnostic: dump EVERY 4-byte field of the active camera object (offset
  // 0..260) once per frame (capped), to the [ml-dump] log, so we can see
  // empirically which fields the game's own right-stick look changes. Only the
  // active hero camera (the one with a non-zero look-at center at +144) is
  // dumped.
  if (cfg.mouse_look_camera_dump) {
    const float cx = fable2::mouselook::read_guest_float(mem, cam + 144);
    if (cx != 0.0f) {
      static int dumped = 0;
      if (dumped < 2400) {
        std::string s = fmt::format("[ml-dump] cam=0x{:08X} ", cam);
        for (uint32_t off = 0; off <= 260; off += 4) {
          const float v = fable2::mouselook::read_guest_float(mem, cam + off);
          s += fmt::format("a{}={:.3f} ", off, v);
        }
        REXSYS_INFO("{}", s.c_str());
        ++dumped;
      }
    }
    // Also dump the camera CONTROLLER (owner object at cam+4) — the native
    // right-stick orbit's accumulated state (angle + clamp) lives here, not on
    // the CECCamera. Only the active hero camera (non-zero eye at +144) is
    // dumped. This lets a full-range stick test reveal the orbit-pitch field
    // and its min/max.
    if (cx != 0.0f) {
      uint32_t owner = 0;
      if (fable2::mouselook::try_read_guest_u32(mem, cam + 4, &owner) &&
          fable2::mouselook::is_plausible_guest_ptr(owner)) {
        static int ctldumped = 0;
        if (ctldumped < 2400) {
          std::string s = fmt::format("[ml-ctl] ctl=0x{:08X} ", owner);
          for (uint32_t off = 0; off <= 256; off += 4) {
            const float v = fable2::mouselook::read_guest_float(mem, owner + off);
            s += fmt::format("c{}={:.3f} ", off, v);
          }
          // The owner's vtable slot-0x74 is the transform UPDATE method (a bctrl
          // in CECCameraUpdateFrame that computes the orbit from the controller
          // state). Resolve its runtime address so we can find and rewrite it via
          // HotFunc (the mouse will drive this exact method, sharing the native
          // orbit bounds).
          {
            uint32_t owner_vtable = 0, vt74 = 0;
            if (fable2::mouselook::try_read_guest_u32(mem, owner + 0, &owner_vtable) &&
                fable2::mouselook::try_read_guest_u32(mem, owner_vtable + 0x74, &vt74)) {
              s += fmt::format(" vt74=0x{:08X}", vt74);
            }
          }
          // Also dump the look sub-object the virtual call reads (owner+124), and
          // resolve the transform method's address: the bctrl in
          // CECCameraUpdateFrame dispatches to *(sub+0)+64 (vtable[16]) with r3 =
          // r1+96. The orbit-angle field we want to offset is somewhere in the
          // sub-object; a full-range stick sweep reveals which field is it.
          uint32_t sub = 0;
          if (fable2::mouselook::try_read_guest_u32(mem, owner + 124, &sub) &&
              fable2::mouselook::is_plausible_guest_ptr(sub)) {
            s += fmt::format(" | sub=0x{:08X} ", sub);
            uint32_t sub_vtable = 0;
            uint32_t vt16 = 0;
            uint32_t vt100 = 0;
            if (fable2::mouselook::try_read_guest_u32(mem, sub + 0, &sub_vtable)) {
              // vtable slot-16 (offset 64): the COPY method (reads the
              // sub-object's transform into the r1+96 buffer).
              if (fable2::mouselook::try_read_guest_u32(mem, sub_vtable + 64, &vt16)) {
                s += fmt::format("vt16=0x{:08X} ", vt16);
              }
              // vtable slot-25 (offset 100): the transform UPDATE method (the
              // second bctrl in CECCameraUpdateFrame, r3 = sub-object, v1 = the
              // lerped transform). This is the function that updates the
              // sub-object's transform from the orbit state.
              if (fable2::mouselook::try_read_guest_u32(mem, sub_vtable + 100, &vt100)) {
                s += fmt::format("vt100=0x{:08X} ", vt100);
              }
            }
            for (uint32_t off = 0; off <= 256; off += 4) {
              const float v = fable2::mouselook::read_guest_float(mem, sub + off);
              s += fmt::format("s{}={:.3f} ", off, v);
            }
          }
          REXSYS_INFO("{}", s.c_str());
          ++ctldumped;
        }
      }
    }
    // Also dump the GLOBAL camera-state object that sub_822CBDC8 integrates the
    // eye offset into (reached via the pointer chain
    // 0x83496940 -> +12 -> +88 -> +4). The look orbit (eye position around the
    // hero) is integrated here, so it may live in this object rather than the
    // CECCamera. Dump it under a distinct [ml-dump] tag.
    uint32_t p0 = 0, p1 = 0, p2 = 0, gs = 0;
    bool ok0 = fable2::mouselook::try_read_guest_u32(mem, 0x83496940u, &p0);
    bool ok1 = ok0 && fable2::mouselook::try_read_guest_u32(mem, p0 + 12, &p1);
    bool ok2 = ok1 && fable2::mouselook::try_read_guest_u32(mem, p1 + 88, &p2);
    bool ok3 = ok2 && fable2::mouselook::try_read_guest_u32(mem, p2 + 4, &gs);
    // Log the pointer chain a few times so we can see where it breaks.
    static int ptrdumped = 0;
    if (ptrdumped < 3) {
      ++ptrdumped;
      REXSYS_INFO("[ml-ptr] base=0x83496940 p0=0x{:08X} p1=0x{:08X} p2=0x{:08X} gs=0x{:08X} ok={}{}{}{}",
                  p0, p1, p2, gs, ok0 ? '0' : 'X', ok1 ? '1' : 'X',
                  ok2 ? '2' : 'X', ok3 ? '3' : 'X');
    }
    if (ok3 && fable2::mouselook::is_plausible_guest_ptr(gs)) {
      static int gsdumped = 0;
      if (gsdumped < 2400) {
        std::string s = fmt::format("[ml-gs] gs=0x{:08X} ", gs);
        for (uint32_t off = 0; off <= 260; off += 4) {
          const float v = fable2::mouselook::read_guest_float(mem, gs + off);
          s += fmt::format("g{}={:.3f} ", off, v);
        }
        REXSYS_INFO("{}", s.c_str());
        ++gsdumped;
      }
    }
  }

  // Diagnostic: log the camera's orbit angle scalars + look-at every frame
  // (capped), so we can watch which fields
  // change when the game's own right-stick look runs. The angle scalars are the
  // camera's accumulated orbit angles: current at +44/48/52, target at +68/72/76.
  if (cfg.mouse_look_camera_diag) {
    static int logged = 0;
    if (logged < 400) {
      auto f = [&](uint32_t addr) {
        return fable2::mouselook::read_guest_float(mem, addr);
      };
      REXSYS_INFO(
          "[ml-d] cam=0x{:08X} ang44={:.3f} ang48={:.3f} ang52={:.3f}  "
          "tgt68={:.3f} tgt72={:.3f} tgt76={:.3f}  center144={:.2f},{:.2f},{:.2f}",
          cam,
          f(cam + 44), f(cam + 48), f(cam + 52),
          f(cam + 68), f(cam + 72), f(cam + 76),
          f(cam + 144), f(cam + 148), f(cam + 152));
      ++logged;
    }
  }

}

// Realtime Texture Morphing (hero/dog black textures without CPU readback;
// plans/hero-dog-realtime-texture-morphing.md). sub_82A76018 turns each
// texture morph request into a morph job and copies the request's
// RealTimeTextureMorphing byte with `lbz r9, 0x20(r30)` at 0x82A7607C. With
// it set, the morph renderer (sub_82A69728) draws straight into the final
// uncompressed texture and builds its mips on the GPU, instead of resolving to
// a scratch texture that the CPU reads back and DXT-compresses (that CPU read
// is what returns black on a split-memory host).
void fable2_hook_realtime_texture_morphing(PPCRegister& r9) {
  if (!fable2::config::Get().realtime_texture_morphing) {
    return;
  }
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXSYS_INFO("[texture-morph] building hero/dog textures in realtime (GPU) mode");
  }
  r9.u64 = 1;
}
