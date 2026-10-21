// Translate the launcher's established Key:Button format to ReXGlue's native
// per-controller keybind cvars. The SDK owns device polling and mouse capture.
#pragma once
#include "keyboard_gamepad.h"
#include <array>
#include <string>
#include <string_view>

namespace fable2::mnkconfig {
inline constexpr std::array<std::string_view, 20> kNames{
    "keybind_a", "keybind_b", "keybind_x", "keybind_y",
    "keybind_left_trigger", "keybind_right_trigger",
    "keybind_left_shoulder", "keybind_right_shoulder",
    "keybind_lstick_up", "keybind_lstick_down", "keybind_lstick_left",
    "keybind_lstick_right", "keybind_lstick_press", "keybind_rstick_press",
    "keybind_dpad_up", "keybind_dpad_down", "keybind_dpad_left",
    "keybind_dpad_right", "keybind_back", "keybind_start"};

inline std::string_view Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  return s;
}

inline int TargetIndex(const input_detail::Mapping& m) {
  using B = rex::input::X_INPUT_GAMEPAD_BUTTON;
  if (m.button == B::X_INPUT_GAMEPAD_A) return 0;
  if (m.button == B::X_INPUT_GAMEPAD_B) return 1;
  if (m.button == B::X_INPUT_GAMEPAD_X) return 2;
  if (m.button == B::X_INPUT_GAMEPAD_Y) return 3;
  if (m.trigger == 'L') return 4;
  if (m.trigger == 'R') return 5;
  if (m.button == B::X_INPUT_GAMEPAD_LEFT_SHOULDER) return 6;
  if (m.button == B::X_INPUT_GAMEPAD_RIGHT_SHOULDER) return 7;
  if (m.axis == 2 && m.axis_sign > 0) return 8;
  if (m.axis == 2 && m.axis_sign < 0) return 9;
  if (m.axis == 1 && m.axis_sign < 0) return 10;
  if (m.axis == 1 && m.axis_sign > 0) return 11;
  if (m.button == B::X_INPUT_GAMEPAD_LEFT_THUMB) return 12;
  if (m.button == B::X_INPUT_GAMEPAD_RIGHT_THUMB) return 13;
  if (m.button == B::X_INPUT_GAMEPAD_DPAD_UP) return 14;
  if (m.button == B::X_INPUT_GAMEPAD_DPAD_DOWN) return 15;
  if (m.button == B::X_INPUT_GAMEPAD_DPAD_LEFT) return 16;
  if (m.button == B::X_INPUT_GAMEPAD_DPAD_RIGHT) return 17;
  if (m.button == B::X_INPUT_GAMEPAD_BACK) return 18;
  if (m.button == B::X_INPUT_GAMEPAD_START) return 19;
  return -1;
}

inline std::array<std::string, kNames.size()> Translate(std::string_view map,
                                                          std::string_view reserved_key) {
  std::array<std::string, kNames.size()> result{};
  // Validate against the same parser used by previous releases.
  for (size_t start = 0; start <= map.size();) {
    const auto comma = map.find(',', start);
    const auto entry = map.substr(start, comma == std::string_view::npos
                                           ? std::string_view::npos : comma - start);
    const auto colon = entry.find(':');
    if (colon != std::string_view::npos) {
      auto key = Trim(entry.substr(0, colon));
      auto guest = Trim(entry.substr(colon + 1));
      const auto vk = input_detail::ParseKey(key);
      if (vk != rex::ui::VirtualKey::kNone &&
          input_detail::ToUpper(key) != input_detail::ToUpper(reserved_key) &&
          input_detail::ToUpper(key) != "F5") {
        input_detail::Mapping mapping{};
        input_detail::FillGuestTarget(mapping, guest);
        int target = TargetIndex(mapping);
        if (target >= 0) {
          std::string upper = input_detail::ToUpper(key);
          // The SDK's key-name table is case-sensitive; map the mouse aliases
          // to the names supplied by the SDK patch.
          std::string canonical;
          if (upper == "XMB1" || upper == "XBUTTON1" || upper == "SIDEBUTTON1")
            canonical = "XMB1";
          else if (upper == "XMB2" || upper == "XBUTTON2" || upper == "SIDEBUTTON2")
            canonical = "XMB2";
          else if (upper == "LMB" || upper == "LEFTMOUSE") canonical = "LMB";
          else if (upper == "RMB" || upper == "RIGHTMOUSE") canonical = "RMB";
          else if (upper == "MMB" || upper == "MIDDLEMOUSE") canonical = "MMB";
          else canonical = std::string(key);
          // The SDK parser knows canonical names; preserve legacy casing of
          // ordinary key names, which the launcher emits consistently.
          if (!result[target].empty()) result[target] += ',';
          result[target] += canonical;
        }
      }
    }
    if (comma == std::string_view::npos) break;
    start = comma + 1;
  }
  return result;
}
}  // namespace fable2::mnkconfig
