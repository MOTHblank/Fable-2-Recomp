// Pure UTF-16 button-markup translation for the recomp UI. Keep this free of
// guest memory and rendering APIs so parsing can be checked independently.
#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace fable2::buttonprompts {

inline std::string Trim(std::string_view v) {
  while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.remove_suffix(1);
  return std::string(v);
}

inline std::string DisplayKey(std::string_view bindings) {
  // ReXGlue bindings are comma-separated alternatives. Prefer a keyboard key
  // over a mouse button, but preserve compact modifier chords such as
  // Shift+Up -> S+Up so D-pad and menu prompts can be represented too.
  auto compact = [](std::string_view raw, bool& mouse_key) -> std::string {
    std::string token = Trim(raw);
    mouse_key = false;
    if (token.empty()) return {};

    std::string prefix;
    std::string_view rest = token;
    for (;;) {
      const size_t plus = rest.find('+');
      if (plus == std::string_view::npos) break;
      std::string mod = Trim(rest.substr(0, plus));
      std::string u;
      for (unsigned char ch : mod) u += static_cast<char>(std::toupper(ch));
      if (u == "SHIFT") prefix += "S+";
      else if (u == "CTRL" || u == "CONTROL") prefix += "C+";
      else if (u == "ALT") prefix += "A+";
      else return {};
      rest.remove_prefix(plus + 1);
    }

    std::string key = Trim(rest);
    if (key.empty()) return {};
    std::string u;
    for (unsigned char ch : key) u += static_cast<char>(std::toupper(ch));
    if (prefix.empty() &&
        (u == "SHIFT" || u == "CTRL" || u == "CONTROL" || u == "ALT")) {
      return {};
    }

    std::string shown = key;
    if (u == "RETURN") shown = "Enter";
    else if (u == "SPACE") shown = "Space";
    else if (u == "ESCAPE") shown = "Esc";
    else if (u == "BACKSPACE") shown = "Bksp";
    else if (u == "SEMICOLON") shown = ";";
    else if (u == "QUOTE") shown = "'";
    else if (u == "LEFT") shown = prefix.empty() ? "Left" : "Lt";
    else if (u == "RIGHT") shown = prefix.empty() ? "Right" : "Rt";
    else if (u == "DOWN") shown = prefix.empty() ? "Down" : "Dn";
    else if (u == "UP") shown = "Up";
    else if (u == "LMB" || u == "RMB" || u == "MMB" ||
             u == "XMB1" || u == "XMB2") {
      mouse_key = true;
      shown = u;
    }

    shown = prefix + shown;
    return shown.size() <= 5 ? shown : std::string{};
  };

  std::string mouse;
  while (!bindings.empty()) {
    const size_t comma = bindings.find(',');
    const std::string_view raw = bindings.substr(0, comma);
    bindings = comma == std::string_view::npos
                   ? std::string_view{}
                   : bindings.substr(comma + 1);
    bool is_mouse = false;
    std::string shown = compact(raw, is_mouse);
    if (shown.empty()) continue;
    if (is_mouse) {
      if (mouse.empty()) mouse = std::move(shown);
      continue;
    }
    return shown;
  }
  return mouse;  // empty means preserve the native Xbox artwork
}

using KeyLabels = std::array<std::string, 4>;  // A, B, X, Y
using ActionLabels = std::array<std::string, 16>;
// A, B, X, Y, LB, RB, LT, RT, Start, Back, LS, RS, Up, Down, Left, Right.

inline std::u16string Rewrite(std::u16string_view original, const KeyLabels& keys) {
  // The title UI's image-tag table uses a/b/c/d; accept x/y aliases as
  // well for strings supplied by other UI assets. Unknown tags pass through.
  constexpr std::array<std::u16string_view, 6> tags{
      u"<a_img>", u"<b_img>", u"<c_img>", u"<d_img>",
      u"<x_img>", u"<y_img>"};
  constexpr std::array<size_t, 6> tag_keys{0, 1, 2, 3, 2, 3};
  std::u16string result;
  result.reserve(original.size());
  for (size_t i = 0; i < original.size();) {
    bool substituted = false;
    for (size_t k = 0; k < tags.size(); ++k) {
      const std::string& label = keys[tag_keys[k]];
      if (label.empty() || label.size() > 5 ||
          original.substr(i, tags[k].size()) != tags[k]) continue;
      // Preserve the full seven-character length of <a_img>. Fable II
      // sometimes retains a separate string-length field while laying out
      // UI; the old shorter replacement inserted an early NUL and could
      // leave cached layout state inconsistent with the source length.
      const size_t pad_left = (5 - label.size()) / 2;
      const size_t pad_right = 5 - label.size() - pad_left;
      result += u'[';
      result.append(pad_left, u' ');
      for (char c : label) result += static_cast<char16_t>(c);
      result.append(pad_right, u' ');
      result += u']';
      i += tags[k].size();
      substituted = true;
      break;
    }
    if (!substituted) result += original[i++];
  }
  return result;
}

// Fixed-width rewrite for legacy/localized text that spells controller inputs
// literally rather than using <x_img> markup. Only conservative English
// instruction forms are touched, and only when the current key label fits in
// the controller token's existing code-unit width. This keeps every guest
// string length and allocation unchanged.
//
// Examples with the default mapping:
//   "Press A to confirm" -> "Press E to confirm"
//   "Hold LT"            -> "Hold Q "
// Mouse-only X/Y bindings ("LMB"/"RMB") do not fit a one-character X/Y token
// and therefore remain untouched here; live GameFace text can relayout those.
inline std::u16string RewriteFixedInstructions(
    std::u16string_view original, const ActionLabels& labels) {
  constexpr std::array<std::u16string_view, 3> verbs{
      u"Press ", u"Hold ", u"Tap "};
  constexpr std::array<std::u16string_view, 12> tokens{
      u"A", u"B", u"X", u"Y", u"LB", u"RB",
      u"LT", u"RT", u"Start", u"Back", u"LS", u"RS"};
  constexpr std::array<size_t, 12> actions{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  auto word = [](char16_t ch) {
    return (ch >= u'0' && ch <= u'9') ||
           (ch >= u'A' && ch <= u'Z') ||
           (ch >= u'a' && ch <= u'z') || ch == u'_';
  };

  std::u16string out{original};
  for (size_t i = 0; i < out.size(); ++i) {
    for (auto verb : verbs) {
      if (i + verb.size() > out.size() ||
          std::u16string_view(out).substr(i, verb.size()) != verb) {
        continue;
      }
      const size_t token_pos = i + verb.size();
      for (size_t t = 0; t < tokens.size(); ++t) {
        const auto token = tokens[t];
        if (token_pos + token.size() > out.size() ||
            std::u16string_view(out).substr(token_pos, token.size()) != token) {
          continue;
        }
        const size_t after = token_pos + token.size();
        if (after < out.size() && word(out[after])) continue;

        const std::string& label = labels[actions[t]];
        if (label.empty() || label.size() > token.size()) continue;
        bool ascii = true;
        for (unsigned char ch : label)
          if (ch < 0x20 || ch > 0x7E) ascii = false;
        if (!ascii) continue;

        for (size_t j = 0; j < token.size(); ++j) {
          out[token_pos + j] =
              j < label.size() ? static_cast<char16_t>(label[j]) : u' ';
        }
        i = token_pos + token.size() - 1;
        break;
      }
      break;
    }
  }
  return out;
}
}  // namespace fable2::buttonprompts
