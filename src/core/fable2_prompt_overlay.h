// Host-side fallback for GameFace button artwork that has no writable text label.
//
// The GameFace adapter hides only unresolved controller icon elements and
// publishes their authored 1280x720 canvas positions here. This dialog draws a
// compact keyboard keycap at the same position. State updates are timestamped;
// stale presentations disappear automatically instead of leaving orphaned
// overlays after a screen transition.
#pragma once

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fable2_button_prompts.h"

REXCVAR_DECLARE(int32_t, ui_prompt_overlay_offset_x);
REXCVAR_DECLARE(int32_t, ui_prompt_overlay_offset_y);
REXCVAR_DECLARE(bool, ui_prompt_legacy_fallback);
REXCVAR_DECLARE(int32_t, ui_prompt_legacy_offset_x);
REXCVAR_DECLARE(int32_t, ui_prompt_legacy_offset_y);

namespace fable2::promptoverlay {

using Clock = std::chrono::steady_clock;

struct Entry {
  std::string action;
  float x = 0.0f;
  float y = 0.0f;
};

struct Presentation {
  std::vector<Entry> entries;
  Clock::time_point updated{};
  // These are acknowledgements from the GameFace Lua adapter, not merely the
  // host's desired cvar state. Keeping them with the entries lets the overlay
  // bridge device/fallback transitions without a one-frame duplicate or gap:
  // draw until GameFace confirms it restored the Xbox icon, and don't draw a
  // new keycap until GameFace confirms it hid that icon.
  bool keyboard_applied = false;
  bool overlay_applied = false;
};

inline std::mutex& StateMutex() {
  static auto* mutex = new std::mutex;
  return *mutex;
}

inline std::unordered_map<uint32_t, Presentation>& States() {
  static auto* states = new std::unordered_map<uint32_t, Presentation>;
  return *states;
}

inline std::atomic<uint8_t>& DesiredGameFaceState() {
  // bit 0 = keyboard, bit 1 = overlay enabled, bit 7 = initialized
  static std::atomic<uint8_t> value{0};
  return value;
}
inline std::atomic<int64_t>& GameFaceTransitionStartedNs() {
  static std::atomic<int64_t> value{0};
  return value;
}

inline int64_t ClockNanoseconds() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

inline void BeginGameFaceTransition(bool keyboard, bool overlay) {
  const uint8_t next = static_cast<uint8_t>(
      0x80u | (keyboard ? 0x01u : 0u) | (overlay ? 0x02u : 0u));
  const uint8_t previous =
      DesiredGameFaceState().exchange(next, std::memory_order_acq_rel);
  if (previous != next) {
    GameFaceTransitionStartedNs().store(ClockNanoseconds(),
                                        std::memory_order_release);
  }
}

inline bool DesiredGameFaceOverlayActive() {
  const uint8_t state =
      DesiredGameFaceState().load(std::memory_order_acquire);
  return (state & 0x83u) == 0x83u;
}

inline bool GameFaceOverlayTransitionGrace() {
  if (DesiredGameFaceOverlayActive()) return true;
  constexpr int64_t kGraceNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::milliseconds(300))
          .count();
  const int64_t started =
      GameFaceTransitionStartedNs().load(std::memory_order_acquire);
  return started != 0 && ClockNanoseconds() - started <= kGraceNs;
}

inline std::atomic<int64_t>& LegacyTitleLastDrawNs() {
  static std::atomic<int64_t> value{0};
  return value;
}
inline std::atomic<int64_t>& LegacyTitleLastElementsNs() {
  static std::atomic<int64_t> value{0};
  return value;
}
inline std::atomic<uint32_t>& LegacyTitleBaselineElements() {
  static std::atomic<uint32_t> value{0};
  return value;
}
inline std::atomic<uint32_t>& LegacyTitleLatestElements() {
  static std::atomic<uint32_t> value{0};
  return value;
}
inline std::atomic<bool>& LegacyTitleKeyboardLayout() {
  static std::atomic<bool> value{false};
  return value;
}
inline std::atomic<bool>& LegacyTitleBakedLayoutConfirmed() {
  static std::atomic<bool> value{false};
  return value;
}
inline std::atomic<int64_t>& LegacyTitleSpriteSuppressedNs() {
  static std::atomic<int64_t> value{0};
  return value;
}
inline std::atomic<uint32_t>& LegacyTitleIconElementPointer() {
  static std::atomic<uint32_t> value{0};
  return value;
}

inline thread_local uint32_t g_legacy_title_frame_elements = 0;
inline thread_local uint32_t g_legacy_title_frame_icon_candidate = 0;
inline thread_local bool g_legacy_title_frame_visible = false;
inline thread_local uint32_t g_legacy_title_scope_depth = 0;

inline void BeginLegacyTitleFrame() {
  if (g_legacy_title_scope_depth++ == 0) {
    g_legacy_title_frame_elements = 0;
    g_legacy_title_frame_icon_candidate = 0;
    g_legacy_title_frame_visible = false;
  }
}

inline void NoteLegacyTitleElement(uint32_t element, bool visible) {
  if (g_legacy_title_scope_depth == 0) return;
  const uint32_t expected =
      fable2::buttonprompts::TitleExpectedIconElementOrdinal();
  if (expected != UINT32_MAX && g_legacy_title_frame_elements == expected) {
    g_legacy_title_frame_icon_candidate = element;
  }
  ++g_legacy_title_frame_elements;
  g_legacy_title_frame_visible = g_legacy_title_frame_visible || visible;
}

inline void EndLegacyTitleFrame() {
  if (g_legacy_title_scope_depth == 0) return;
  if (--g_legacy_title_scope_depth != 0) return;
  const int64_t now = ClockNanoseconds();
  const uint32_t count = g_legacy_title_frame_elements;
  if (count == 0) {
    constexpr int64_t kResetNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::seconds(2))
            .count();
    const int64_t last =
        LegacyTitleLastElementsNs().load(std::memory_order_acquire);
    if (last != 0 && now - last > kResetNs) {
      LegacyTitleBaselineElements().store(0, std::memory_order_release);
      LegacyTitleLatestElements().store(0, std::memory_order_release);
      LegacyTitleKeyboardLayout().store(false, std::memory_order_release);
      LegacyTitleBakedLayoutConfirmed().store(false,
                                              std::memory_order_release);
      LegacyTitleIconElementPointer().store(0, std::memory_order_release);
    }
    return;
  }

  LegacyTitleLastElementsNs().store(now, std::memory_order_release);
  LegacyTitleLatestElements().store(count, std::memory_order_release);

  uint32_t baseline =
      LegacyTitleBaselineElements().load(std::memory_order_acquire);
  const bool source_rewritten =
      fable2::buttonprompts::TitleSourceRewriteActive();
  const uint32_t expected_baked =
      fable2::buttonprompts::TitleExpectedBakedElementCount();
  const uint32_t expected_keyboard =
      fable2::buttonprompts::TitleExpectedKeyboardElementCount();

  // UITextPrompt_Render is generic enough that other screens may invoke it.
  // Only a layout matching the title string discovered by the source scanner
  // is allowed to drive the fixed-position title fallback.
  const auto near_count = [count](uint32_t expected) {
    if (expected == 0) return false;
    const uint32_t delta = count > expected ? count - expected : expected - count;
    return delta <= 1;
  };
  const bool have_expectations =
      expected_baked != 0 && expected_keyboard > expected_baked;
  const bool title_layout =
      !have_expectations ||
      near_count(expected_baked) || near_count(expected_keyboard);
  if (!title_layout) {
    return;
  }

  // Prefer exact expectations derived from the localized source string. This
  // handles English and every supported translation without hardcoded 16/22
  // counts, and distinguishes a late scanner hit from a layout that actually
  // consumed the rewritten keycap text.
  if (have_expectations) {
    if (count + 1 >= expected_keyboard) {
      LegacyTitleKeyboardLayout().store(true, std::memory_order_release);
      LegacyTitleBakedLayoutConfirmed().store(false,
                                              std::memory_order_release);
      LegacyTitleIconElementPointer().store(0, std::memory_order_release);
    } else if (count <= expected_baked + 1) {
      LegacyTitleKeyboardLayout().store(false, std::memory_order_release);
      // Element suppression is allowed only when the localized source scanner
      // also identified the exact image-tag ordinal. Count-only fallback is
      // intentionally not enough to skip a guest renderer element.
      const bool exact_icon =
          fable2::buttonprompts::TitleExpectedIconElementOrdinal() !=
              UINT32_MAX &&
          g_legacy_title_frame_icon_candidate != 0;
      LegacyTitleBakedLayoutConfirmed().store(exact_icon,
                                              std::memory_order_release);
      LegacyTitleIconElementPointer().store(
          exact_icon ? g_legacy_title_frame_icon_candidate : 0,
          std::memory_order_release);
    } else {
      LegacyTitleBakedLayoutConfirmed().store(false,
                                              std::memory_order_release);
      LegacyTitleIconElementPointer().store(0, std::memory_order_release);
    }
    LegacyTitleBaselineElements().store(expected_baked,
                                        std::memory_order_release);
  } else if (baseline == 0) {
    // The scanner has not identified the localized source yet. Preserve the
    // first observed structure as a fallback baseline.
    LegacyTitleBaselineElements().store(count, std::memory_order_release);
    LegacyTitleKeyboardLayout().store(false, std::memory_order_release);
    LegacyTitleBakedLayoutConfirmed().store(false,
                                            std::memory_order_release);
    LegacyTitleIconElementPointer().store(0, std::memory_order_release);
  } else if (!source_rewritten) {
    if (count < baseline) {
      LegacyTitleBaselineElements().store(count, std::memory_order_release);
    }
    LegacyTitleKeyboardLayout().store(false, std::memory_order_release);
    LegacyTitleBakedLayoutConfirmed().store(false,
                                            std::memory_order_release);
    LegacyTitleIconElementPointer().store(0, std::memory_order_release);
  } else if (count >= baseline + 4) {
    // Unknown localization/source: a seven-unit image tag becoming ordinary
    // glyphs still creates a clear multi-element increase.
    LegacyTitleKeyboardLayout().store(true, std::memory_order_release);
    LegacyTitleBakedLayoutConfirmed().store(false,
                                            std::memory_order_release);
    LegacyTitleIconElementPointer().store(0, std::memory_order_release);
  }

  if (g_legacy_title_frame_visible && title_layout) {
    LegacyTitleLastDrawNs().store(now, std::memory_order_release);
  }
}

inline bool LegacyTitleRecentlyDrawn() {
  constexpr int64_t kAliveNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::milliseconds(110))
          .count();
  const int64_t last =
      LegacyTitleLastDrawNs().load(std::memory_order_acquire);
  return last != 0 && ClockNanoseconds() - last <= kAliveNs;
}

inline bool LegacyTitleUsesKeyboardLayout() {
  return LegacyTitleKeyboardLayout().load(std::memory_order_acquire);
}

inline uint32_t CurrentLegacyTitleElementOrdinal() {
  return g_legacy_title_frame_elements;
}

inline bool ShouldSuppressLegacyTitleElement(uint32_t ordinal,
                                                   uint32_t element) {
  if (g_legacy_title_scope_depth == 0 ||
      !LegacyTitleBakedLayoutConfirmed().load(std::memory_order_acquire) ||
      !LegacyTitleRecentlyDrawn() ||
      !fable2::buttonprompts::KeyboardPromptModeActive() ||
      !REXCVAR_GET(ui_prompt_legacy_fallback)) {
    return false;
  }
  const uint32_t expected =
      fable2::buttonprompts::TitleExpectedIconElementOrdinal();
  const uint32_t confirmed_element =
      LegacyTitleIconElementPointer().load(std::memory_order_acquire);
  return expected != UINT32_MAX && ordinal == expected &&
         confirmed_element != 0 && element == confirmed_element;
}

inline void NoteLegacyTitleSpriteSuppressed() {
  LegacyTitleSpriteSuppressedNs().store(ClockNanoseconds(),
                                        std::memory_order_release);
}

inline bool LegacyTitleSpriteRecentlySuppressed() {
  constexpr int64_t kAliveNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::milliseconds(110))
          .count();
  const int64_t last =
      LegacyTitleSpriteSuppressedNs().load(std::memory_order_acquire);
  return last != 0 && ClockNanoseconds() - last <= kAliveNs;
}

inline std::string_view LegacyTitleActionName() {
  switch (fable2::buttonprompts::TitleExpectedIconActionIndex()) {
    case 0: return "A";
    case 1: return "B";
    case 2: return "X";
    case 3: return "Y";
    default: return "A";
  }
}

inline bool ParseFloat(std::string_view text, float& out) {
  if (text.empty() || text.size() > 40) return false;
  std::string copy{text};
  char* end = nullptr;
  const float value = std::strtof(copy.c_str(), &end);
  if (!end || end != copy.c_str() + copy.size() || !std::isfinite(value)) {
    return false;
  }
  out = value;
  return true;
}

inline void UpdatePresentation(uint32_t lua_state, std::string_view result) {
  constexpr std::string_view kPrefix = "ok __f2_prompt_overlay|";
  if (!result.starts_with(kPrefix)) return;
  result.remove_prefix(kPrefix.size());

  // Wire format: "<mode><overlay>|entries", where mode is k/c and overlay is
  // 1/0. This is emitted only after the Lua adapter has restored/applied the
  // corresponding GameFace attributes, so it doubles as a transition ack.
  if (result.size() < 3 || (result[0] != 'k' && result[0] != 'c') ||
      (result[1] != '0' && result[1] != '1') || result[2] != '|') {
    return;
  }
  const bool keyboard_applied = result[0] == 'k';
  const bool overlay_applied = result[1] == '1';
  result.remove_prefix(3);

  std::vector<Entry> parsed;
  while (!result.empty() && parsed.size() < 128) {
    const size_t semi = result.find(';');
    std::string_view item = result.substr(0, semi);
    result = semi == std::string_view::npos ? std::string_view{}
                                             : result.substr(semi + 1);
    if (item.empty()) continue;

    const size_t c1 = item.find(',');
    const size_t c2 = c1 == std::string_view::npos
                          ? std::string_view::npos
                          : item.find(',', c1 + 1);
    if (c1 == std::string_view::npos || c2 == std::string_view::npos) continue;

    Entry entry;
    entry.action.assign(item.substr(0, c1));
    if (entry.action.empty() || entry.action.size() > 8) continue;
    if (!ParseFloat(item.substr(c1 + 1, c2 - c1 - 1), entry.x) ||
        !ParseFloat(item.substr(c2 + 1), entry.y)) {
      continue;
    }
    // Ignore corrupt transforms before they can generate pathological ImGui
    // geometry. Legitimate Fable UI can sit outside the 1280x720 viewport
    // during animations, so keep a generous authored-canvas bound.
    if (std::abs(entry.x) > 8192.0f || std::abs(entry.y) > 8192.0f) continue;
    parsed.push_back(std::move(entry));
  }

  std::lock_guard lock(StateMutex());
  States()[lua_state] = Presentation{
      std::move(parsed), Clock::now(), keyboard_applied, overlay_applied};
}

inline void Clear() {
  {
    std::lock_guard lock(StateMutex());
    States().clear();
  }
  LegacyTitleLastDrawNs().store(0, std::memory_order_release);
  LegacyTitleLastElementsNs().store(0, std::memory_order_release);
  LegacyTitleBaselineElements().store(0, std::memory_order_release);
  LegacyTitleLatestElements().store(0, std::memory_order_release);
  LegacyTitleKeyboardLayout().store(false, std::memory_order_release);
  LegacyTitleBakedLayoutConfirmed().store(false, std::memory_order_release);
  LegacyTitleSpriteSuppressedNs().store(0, std::memory_order_release);
  LegacyTitleIconElementPointer().store(0, std::memory_order_release);
  DesiredGameFaceState().store(0, std::memory_order_release);
  GameFaceTransitionStartedNs().store(0, std::memory_order_release);
}

inline std::vector<Entry> Snapshot() {
  constexpr auto kAliveFor = std::chrono::milliseconds(2200);
  const auto now = Clock::now();
  std::vector<Entry> out;
  std::lock_guard lock(StateMutex());
  for (auto it = States().begin(); it != States().end();) {
    if (now - it->second.updated > kAliveFor) {
      it = States().erase(it);
      continue;
    }
    // Entries describe icons the adapter has actually hidden. Ignore host
    // desired state here: during keyboard->controller or fallback-off handoff,
    // continuing to draw these old entries is what covers the icon until the
    // controller/restore acknowledgement arrives.
    if (!it->second.keyboard_applied || !it->second.overlay_applied) {
      ++it;
      continue;
    }
    for (const Entry& entry : it->second.entries) {
      bool duplicate = false;
      for (const Entry& existing : out) {
        if (existing.action == entry.action &&
            std::abs(existing.x - entry.x) < 4.0f &&
            std::abs(existing.y - entry.y) < 4.0f) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) out.push_back(entry);
    }
    ++it;
  }
  return out;
}

inline std::string ActionLabel(std::string_view action) {
  const char* cvar = nullptr;
  if (action == "A") cvar = "keybind_a";
  else if (action == "B") cvar = "keybind_b";
  else if (action == "X") cvar = "keybind_x";
  else if (action == "Y") cvar = "keybind_y";
  else if (action == "LB") cvar = "keybind_left_shoulder";
  else if (action == "RB") cvar = "keybind_right_shoulder";
  else if (action == "LT") cvar = "keybind_left_trigger";
  else if (action == "RT") cvar = "keybind_right_trigger";
  else if (action == "START") cvar = "keybind_start";
  else if (action == "BACK") cvar = "keybind_back";
  else if (action == "LS") cvar = "keybind_lstick_press";
  else if (action == "RS") cvar = "keybind_rstick_press";
  else if (action == "UP") cvar = "keybind_dpad_up";
  else if (action == "DOWN") cvar = "keybind_dpad_down";
  else if (action == "LEFT") cvar = "keybind_dpad_left";
  else if (action == "RIGHT") cvar = "keybind_dpad_right";
  return cvar ? fable2::buttonprompts::DisplayKey(rex::cvar::GetFlagByName(cvar))
              : std::string{};
}

inline bool KeyboardModeActive() {
  return fable2::buttonprompts::KeyboardPromptModeActive();
}

class PromptOverlayDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit PromptOverlayDialog(rex::ui::ImGuiDrawer* drawer)
      : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
      return;
    }

    const bool desired_keyboard = KeyboardModeActive();
    // GameFace overlay entries are acknowledgement-driven (Snapshot()), not
    // desired-state-driven. This keeps the old keycap covering an icon until
    // GameFace restores it, and waits to draw a new keycap until GameFace has
    // hidden the Xbox artwork. The legacy title cover never hides guest art,
    // so it can still follow desired mode immediately.
    const bool legacy_fallback =
        desired_keyboard && REXCVAR_GET(ui_prompt_legacy_fallback) &&
        LegacyTitleRecentlyDrawn() &&
        !LegacyTitleUsesKeyboardLayout();

    const std::vector<Entry> entries =
        GameFaceOverlayTransitionGrace() ? Snapshot() : std::vector<Entry>{};
    if (entries.empty() && !legacy_fallback) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    if (!draw || !font) return;

    // GameFace's authored canvas is 1280x720. For a live display aspect A the
    // visible horizontal canvas is [640 - 360A, 640 + 360A], while Y remains
    // 0..720. This is the same transform used by fable2_fit_window_ui.h.
    const float aspect = io.DisplaySize.x / io.DisplaySize.y;
    const float canvas_left = 640.0f - 360.0f * aspect;
    const float canvas_to_screen = io.DisplaySize.y / 720.0f;

    const float font_size = std::max(11.0f, ImGui::GetFontSize() * 0.86f);
    constexpr ImU32 kFill = IM_COL32(28, 25, 21, 232);
    constexpr ImU32 kBorder = IM_COL32(225, 211, 177, 235);
    constexpr ImU32 kText = IM_COL32(247, 239, 216, 255);
    constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 105);

    const float nudge_x =
        static_cast<float>(REXCVAR_GET(ui_prompt_overlay_offset_x));
    const float nudge_y =
        static_cast<float>(REXCVAR_GET(ui_prompt_overlay_offset_y));

    auto key_width = [&](std::string_view part) {
      const ImVec2 size = font->CalcTextSizeA(
          font_size, 1000.0f, 0.0f, part.data(), part.data() + part.size());
      return std::max(24.0f, size.x + 12.0f);
    };

    for (const Entry& entry : entries) {
      const std::string label = ActionLabel(entry.action);
      if (label.empty()) continue;

      const float cx = (entry.x - canvas_left) * canvas_to_screen + nudge_x;
      const float cy = entry.y * canvas_to_screen + nudge_y;
      if (cx < -96.0f || cy < -64.0f ||
          cx > io.DisplaySize.x + 96.0f || cy > io.DisplaySize.y + 64.0f) {
        continue;
      }

      std::vector<std::string_view> parts;
      std::string_view remaining = label;
      while (!remaining.empty() && parts.size() < 3) {
        const size_t plus = remaining.find('+');
        parts.push_back(remaining.substr(0, plus));
        if (plus == std::string_view::npos) break;
        remaining.remove_prefix(plus + 1);
      }
      if (parts.empty()) continue;

      const ImVec2 plus_size =
          font->CalcTextSizeA(font_size, 1000.0f, 0.0f, "+");
      constexpr float kGap = 5.0f;
      float total_w = 0.0f;
      for (std::string_view part : parts) total_w += key_width(part);
      if (parts.size() > 1) {
        total_w += static_cast<float>(parts.size() - 1) *
                   (plus_size.x + kGap * 2.0f);
      }

      float cursor = cx - total_w * 0.5f;
      for (size_t i = 0; i < parts.size(); ++i) {
        const std::string_view part = parts[i];
        const ImVec2 text_size = font->CalcTextSizeA(
            font_size, 1000.0f, 0.0f, part.data(), part.data() + part.size());
        const float box_w = key_width(part);
        const float box_h = std::max(20.0f, text_size.y + 7.0f);
        const ImVec2 p0(cursor, cy - box_h * 0.5f);
        const ImVec2 p1(cursor + box_w, cy + box_h * 0.5f);

        draw->AddRectFilled(ImVec2(p0.x + 1.5f, p0.y + 2.0f),
                            ImVec2(p1.x + 1.5f, p1.y + 2.0f),
                            kShadow, 4.0f);
        draw->AddRectFilled(p0, p1, kFill, 4.0f);
        draw->AddRect(p0, p1, kBorder, 4.0f, 0, 1.0f);
        draw->AddText(font, font_size,
                      ImVec2(cursor + (box_w - text_size.x) * 0.5f,
                             cy - text_size.y * 0.5f),
                      kText, part.data(), part.data() + part.size());
        cursor += box_w;

        if (i + 1 < parts.size()) {
          cursor += kGap;
          draw->AddText(font, font_size,
                        ImVec2(cursor, cy - plus_size.y * 0.5f),
                        kBorder, "+");
          cursor += plus_size.x + kGap;
        }
      }
    }

    if (legacy_fallback) {
      const std::string label = ActionLabel(LegacyTitleActionName());
      if (!label.empty()) {
        const float cx = (640.0f - canvas_left) * canvas_to_screen +
                         static_cast<float>(
                             REXCVAR_GET(ui_prompt_legacy_offset_x));
        const float cy = 560.0f * canvas_to_screen +
                         static_cast<float>(
                             REXCVAR_GET(ui_prompt_legacy_offset_y));
        const ImVec2 text_size =
            font->CalcTextSizeA(font_size, 1000.0f, 0.0f, label.c_str());
        // Opaque and a little larger than the GameFace fallback: this path is
        // intentionally a cover for an already-baked Xbox face-button sprite.
        const float box_w = std::max(34.0f, text_size.x + 16.0f);
        const float box_h = std::max(27.0f, text_size.y + 10.0f);
        const ImVec2 p0(cx - box_w * 0.5f, cy - box_h * 0.5f);
        const ImVec2 p1(cx + box_w * 0.5f, cy + box_h * 0.5f);
        constexpr ImU32 kLegacyFill = IM_COL32(28, 25, 21, 255);
        draw->AddRectFilled(ImVec2(p0.x + 1.5f, p0.y + 2.0f),
                            ImVec2(p1.x + 1.5f, p1.y + 2.0f),
                            kShadow, 4.0f);
        draw->AddRectFilled(p0, p1, kLegacyFill, 4.0f);
        draw->AddRect(p0, p1, kBorder, 4.0f, 0, 1.2f);
        draw->AddText(font, font_size,
                      ImVec2(cx - text_size.x * 0.5f,
                             cy - text_size.y * 0.5f),
                      kText, label.c_str());
      }
    }
  }
};

}  // namespace fable2::promptoverlay
