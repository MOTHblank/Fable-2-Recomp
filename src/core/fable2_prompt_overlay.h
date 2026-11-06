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

#include "fable2_button_prompt_strings.h"

REXCVAR_DECLARE(int32_t, ui_prompt_overlay_offset_x);
REXCVAR_DECLARE(int32_t, ui_prompt_overlay_offset_y);

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
};

inline std::mutex& StateMutex() {
  static auto* mutex = new std::mutex;
  return *mutex;
}

inline std::unordered_map<uint32_t, Presentation>& States() {
  static auto* states = new std::unordered_map<uint32_t, Presentation>;
  return *states;
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
  States()[lua_state] = Presentation{std::move(parsed), Clock::now()};
}

inline void Clear() {
  std::lock_guard lock(StateMutex());
  States().clear();
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
  const std::string mode = rex::cvar::GetFlagByName("ui_prompt_mode");
  if (mode == "controller") return false;
  if (mode == "keyboard") return true;
  return rex::cvar::GetFlagByName("input_prompt_keyboard") != "false";
}

class PromptOverlayDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit PromptOverlayDialog(rex::ui::ImGuiDrawer* drawer)
      : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (rex::cvar::GetFlagByName("ui_prompt_overlay_fallback") != "true" ||
        !KeyboardModeActive() || io.DisplaySize.x <= 0.0f ||
        io.DisplaySize.y <= 0.0f) {
      return;
    }

    const std::vector<Entry> entries = Snapshot();
    if (entries.empty()) return;

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
  }
};

}  // namespace fable2::promptoverlay
