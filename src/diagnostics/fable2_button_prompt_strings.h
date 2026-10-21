// Pure UTF-16 button-markup translation for the recomp UI. Keep this free of
// guest memory and rendering APIs so parsing can be checked independently.
#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace fable2::buttonprompts {

inline std::string Trim(std::string_view v) {
  while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.remove_suffix(1);
  return std::string(v);
}

inline std::string DisplayKey(std::string_view bindings) {
  // The native ReXGlue binding is an ordered comma-separated list of keys.
  // Prefer a normal key over a mouse button, and skip bare modifiers (the
  // native driver's modifier-matching rules make bare Shift/Ctrl unusable).
  std::string mouse;
  while (!bindings.empty()) {
    const size_t comma = bindings.find(',');
    const std::string key = Trim(bindings.substr(0, comma));
    bindings = comma == std::string_view::npos ? std::string_view{} :
                                                    bindings.substr(comma + 1);
    if (key.empty() || key.find('+') != std::string::npos) continue;
    std::string u;
    for (unsigned char c : key) u += static_cast<char>(std::toupper(c));
    if (u == "SHIFT" || u == "CTRL" || u == "CONTROL" || u == "ALT") continue;
    std::string shown = key;
    if (u == "RETURN") shown = "Enter";
    else if (u == "SPACE") shown = "Space";
    else if (u == "ESCAPE") shown = "Esc";
    else if (u == "BACKSPACE") shown = "Bksp";
    else if (u == "SEMICOLON") shown = ";";
    else if (u == "QUOTE") shown = "'";
    else if (u == "LMB" || u == "RMB" || u == "MMB" ||
             u == "XMB1" || u == "XMB2") {
      if (mouse.empty()) mouse = u;
      continue;
    }
    if (shown.size() <= 5) return shown;
  }
  return mouse;  // empty means leave the original Xbox icon untouched
}

using KeyLabels = std::array<std::string, 4>;  // A, B, X, Y

inline std::u16string Rewrite(std::u16string_view original, const KeyLabels& keys) {
  constexpr std::array<std::u16string_view, 4> tags{
      u"<a_img>", u"<b_img>", u"<x_img>", u"<y_img>"};
  std::u16string result;
  result.reserve(original.size());
  for (size_t i = 0; i < original.size();) {
    bool substituted = false;
    for (size_t k = 0; k < tags.size(); ++k) {
      if (keys[k].empty() || keys[k].size() > 5 ||
          original.substr(i, tags[k].size()) != tags[k]) continue;
      result += u'[';
      for (char c : keys[k]) result += static_cast<char16_t>(c);
      result += u']';
      i += tags[k].size();
      substituted = true;
      break;
    }
    if (!substituted) result += original[i++];
  }
  return result;
}
}  // namespace fable2::buttonprompts
