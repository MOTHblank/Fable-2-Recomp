// Legacy Key:Button mapping parser. Fable now uses the SDK's event-driven
// MnkInputDriver; this header only preserves config parsing and aliases.
#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <rex/input/input.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/keybinds.h>

namespace fable2 {
namespace input_detail {

// Full thumbstick deflection (XInput range is -32768..32767).
constexpr int32_t kStickMax = 32767;

inline int32_t ClampStick(int32_t v) {
  if (v > kStickMax) return kStickMax;
  if (v < -kStickMax) return -kStickMax;
  return v;
}

// One host-key -> guest-input binding. Exactly one of button/trigger/axis
// is set.
struct Mapping {
  uint16_t vk = 0;       // host virtual-key code (0 == invalid)
  uint16_t button = 0;   // guest X_INPUT_GAMEPAD_* button mask
  char trigger = 0;      // 0 = not a trigger, 'L' = left, 'R' = right
  int8_t axis = 0;       // 0 = not an axis, 1 = left-stick X, 2 = left-stick Y
  int8_t axis_sign = 0;  // +1 / -1 deflection direction for the axis
  bool valid() const { return button != 0 || trigger != 0 || axis != 0; }
};

// Case-insensitive guest-input name -> mapping fragment. Recognizes buttons,
// triggers, and left-stick axis names. Unknown names reset the fragment to
// invalid.
inline void FillGuestTarget(Mapping& m, std::string_view name) {
  auto upper = [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };
  std::string_view n = name;
  while (!n.empty() && (n.front() == ' ' || n.front() == '\t')) n.remove_prefix(1);
  while (!n.empty() && (n.back() == ' ' || n.back() == '\t')) n.remove_suffix(1);
  std::string u;
  u.reserve(n.size());
  for (char c : n) u.push_back(upper(c));

  using B = rex::input::X_INPUT_GAMEPAD_BUTTON;
  if (u == "A") {
    m.button = B::X_INPUT_GAMEPAD_A;
  } else if (u == "B") {
    m.button = B::X_INPUT_GAMEPAD_B;
  } else if (u == "X") {
    m.button = B::X_INPUT_GAMEPAD_X;
  } else if (u == "Y") {
    m.button = B::X_INPUT_GAMEPAD_Y;
  } else if (u == "LB" || u == "LSHOULDER") {
    m.button = B::X_INPUT_GAMEPAD_LEFT_SHOULDER;
  } else if (u == "RB" || u == "RSHOULDER") {
    m.button = B::X_INPUT_GAMEPAD_RIGHT_SHOULDER;
  } else if (u == "UP" || u == "DUP") {
    m.button = B::X_INPUT_GAMEPAD_DPAD_UP;
  } else if (u == "DOWN" || u == "DDOWN") {
    m.button = B::X_INPUT_GAMEPAD_DPAD_DOWN;
  } else if (u == "LEFT" || u == "DLEFT") {
    m.button = B::X_INPUT_GAMEPAD_DPAD_LEFT;
  } else if (u == "RIGHT" || u == "DRIGHT") {
    m.button = B::X_INPUT_GAMEPAD_DPAD_RIGHT;
  } else if (u == "START" || u == "PAUSE") {
    m.button = B::X_INPUT_GAMEPAD_START;
  } else if (u == "BACK" || u == "VIEW" || u == "SELECT") {
    m.button = B::X_INPUT_GAMEPAD_BACK;
  } else if (u == "L3" || u == "LTHUMB") {
    m.button = B::X_INPUT_GAMEPAD_LEFT_THUMB;
  } else if (u == "R3" || u == "RTHUMB") {
    m.button = B::X_INPUT_GAMEPAD_RIGHT_THUMB;
  } else if (u == "LT" || u == "LTRIGGER") {
    m.trigger = 'L';
  } else if (u == "RT" || u == "RTRIGGER") {
    m.trigger = 'R';
  // Left stick axes. lx negative = left (standard). Fable 2 reads ly positive
  // as forward/up (opposite of the standard XInput "ly negative = up"), so the
  // Y signs are flipped to match the game's expectation.
  } else if (u == "STICKUP" || u == "LU") {
    m.axis = 2; m.axis_sign = 1;
  } else if (u == "STICKDOWN" || u == "LD") {
    m.axis = 2; m.axis_sign = -1;
  } else if (u == "STICKLEFT" || u == "LL") {
    m.axis = 1; m.axis_sign = -1;
  } else if (u == "STICKRIGHT" || u == "LR") {
    m.axis = 1; m.axis_sign = 1;
  } else {
    m = Mapping{};  // unknown -> invalid
  }
}

// Case-insensitive upper helper (shared by the name parsers below).
inline std::string ToUpper(std::string_view s) {
  std::string u;
  u.reserve(s.size());
  for (char c : s) u.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  return u;
}

// Host key-name -> VirtualKey. The SDK table (rex::ui::ParseVirtualKey) is
// authoritative and already recognizes LMB/RMB/MMB; this fallback additionally
// accepts mouse-button names case-insensitively ("lmb") and covers the side
// buttons the SDK table does not spell out (VK_XBUTTON1/2). Returns kNone when
// the name is not a known key.
inline rex::ui::VirtualKey ParseKey(std::string_view name) {
  // Tolerate stray spaces/tabs around the name ("LMB", " LMB ").
  while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.remove_prefix(1);
  while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.remove_suffix(1);
  auto vk = rex::ui::ParseVirtualKey(name);
  if (vk != rex::ui::VirtualKey::kNone) return vk;
  using V = rex::ui::VirtualKey;
  std::string u = ToUpper(name);
  if (u == "LMB" || u == "LEFTMOUSE") return V::kLButton;   // VK_LBUTTON
  if (u == "RMB" || u == "RIGHTMOUSE") return V::kRButton;  // VK_RBUTTON
  if (u == "MMB" || u == "MIDDLEMOUSE") return V::kMButton;  // VK_MBUTTON
  if (u == "XMB1" || u == "XBUTTON1" || u == "SIDEBUTTON1")
    return V::kXButton1;  // mouse side button 1
  if (u == "XMB2" || u == "XBUTTON2" || u == "SIDEBUTTON2")
    return V::kXButton2;  // mouse side button 2
  return V::kNone;
}

// Parse "Key:Button,Key:Button,...". Entries whose key or target name does not
// parse are skipped. Returns an empty vector on empty input.
inline std::vector<Mapping> ParseMap(std::string_view text) {
  std::vector<Mapping> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    std::string_view entry =
        text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
    if (!entry.empty()) {
      size_t colon = entry.find(':');
      if (colon != std::string_view::npos) {
        auto vk = ParseKey(entry.substr(0, colon));
        if (vk != rex::ui::VirtualKey::kNone) {
          Mapping m;
          m.vk = static_cast<uint16_t>(vk);
          FillGuestTarget(m, entry.substr(colon + 1));
          if (m.valid()) out.push_back(m);
        }
      }
    }
    if (comma == std::string_view::npos) break;
    start = comma + 1;
  }
  return out;
}

}  // namespace input_detail
}  // namespace fable2
