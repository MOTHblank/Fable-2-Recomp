// Reversible, bounded host-side replacement of known UTF-16BE UI markup.
// Fable II rebuilds front-end text from writable heap strings. Do not scan or
// allocate on the render thread; only replace whole, verified source strings.
// Unknown markup and controller mode are left exactly as the game supplied.
#pragma once
#include "fable2_button_prompt_strings.h"
#include <rex/cvar.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <mutex>
#include <iterator>
#include <utility>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fable2::buttonprompts {

using GuiSyncCallback = void (*)(bool keyboard);
inline std::atomic<GuiSyncCallback>& GuiSync() {
  static std::atomic<GuiSyncCallback> callback{nullptr};
  return callback;
}
inline void SetGuiSyncCallback(GuiSyncCallback callback) {
  GuiSync().store(callback, std::memory_order_release);
}
inline void SyncGraphicalPrompts(bool keyboard) {
  if (auto callback = GuiSync().load(std::memory_order_acquire)) callback(keyboard);
}

// Single source of truth for the requested presentation mode. The source
// scanner, GameFace synchronization and host fallback all use this decision;
// "auto" follows the SDK's last meaningful physical input.
inline bool KeyboardPromptModeActive() {
  const std::string mode = rex::cvar::GetFlagByName("ui_prompt_mode");
  if (mode == "controller") return false;
  if (mode == "keyboard") return true;
  return rex::cvar::GetFlagByName("input_prompt_keyboard") != "false";
}

#ifdef _WIN32
struct Patched {
  uint8_t* address;
  std::vector<uint8_t> original;
  std::vector<uint8_t> changed;
  bool title = false;
};

inline std::atomic<bool>& TitleSourceActive() {
  static std::atomic<bool> active{false};
  return active;
}

inline bool TitleSourceRewriteActive() {
  return TitleSourceActive().load(std::memory_order_acquire);
}

inline std::atomic<uint32_t>& TitleExpectedBakedElements() {
  static std::atomic<uint32_t> value{0};
  return value;
}
inline std::atomic<uint32_t>& TitleExpectedKeyboardElements() {
  static std::atomic<uint32_t> value{0};
  return value;
}
// Baked UITextPrompt element ordinal and face-action index for the first
// known image tag in the localized title source. UINT32_MAX means the scanner
// has not observed enough source markup to identify the sprite safely.
inline std::atomic<uint32_t>& TitleExpectedIconElement() {
  static std::atomic<uint32_t> value{UINT32_MAX};
  return value;
}
inline std::atomic<uint32_t>& TitleExpectedIconAction() {
  static std::atomic<uint32_t> value{UINT32_MAX};
  return value;
}
inline uint32_t TitleExpectedBakedElementCount() {
  return TitleExpectedBakedElements().load(std::memory_order_acquire);
}
inline uint32_t TitleExpectedKeyboardElementCount() {
  return TitleExpectedKeyboardElements().load(std::memory_order_acquire);
}
inline uint32_t TitleExpectedIconElementOrdinal() {
  return TitleExpectedIconElement().load(std::memory_order_acquire);
}
inline uint32_t TitleExpectedIconActionIndex() {
  return TitleExpectedIconAction().load(std::memory_order_acquire);
}

inline bool Writable(const void* pointer, size_t count) {
  MEMORY_BASIC_INFORMATION m{};
  if (!VirtualQuery(pointer, &m, sizeof m) || m.State != MEM_COMMIT ||
      (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) == 0 ||
      (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
  const uintptr_t p = reinterpret_cast<uintptr_t>(pointer);
  const uintptr_t lo = reinterpret_cast<uintptr_t>(m.BaseAddress);
  return p >= lo && count <= m.RegionSize && p - lo <= m.RegionSize - count;
}

inline bool Same(const Patched& patch) {
  return Writable(patch.address, patch.changed.size()) &&
         std::memcmp(patch.address, patch.changed.data(), patch.changed.size()) == 0;
}

inline void RefreshTitleSourceState(const std::vector<Patched>& patches) {
  bool active = false;
  for (const auto& patch : patches) {
    if (patch.title && Same(patch)) {
      active = true;
      break;
    }
  }
  TitleSourceActive().store(active, std::memory_order_release);
}

// Only restore bytes if they're still exactly the ones we wrote. The guest can
// free or reuse a heap allocation at any time; never write to stale contents.
inline void Restore(std::vector<Patched>& patches) {
  for (auto& p : patches)
    if (Same(p)) std::memcpy(p.address, p.original.data(), p.original.size());
  patches.clear();
  TitleSourceActive().store(false, std::memory_order_release);
}

inline bool TextUnit(uint16_t codepoint) {
  // Printable ASCII, accented Latin and Cyrillic used by supported regions.
  // Reject arbitrary binary heap data and unsupported UTF-16 surrogates.
  return (codepoint >= 0x20 && codepoint <= 0x7E) ||
         (codepoint >= 0xA0 && codepoint <= 0x052F);
}

inline uint16_t Be16(const uint8_t* p) {
  return static_cast<uint16_t>((uint16_t(p[0]) << 8) | p[1]);
}

inline bool BeAsciiAt(const uint8_t* p, size_t bytes, std::string_view ascii) {
  if (bytes < ascii.size() * 2) return false;
  for (size_t i = 0; i < ascii.size(); ++i) {
    if (p[i * 2] != 0 ||
        p[i * 2 + 1] != static_cast<uint8_t>(ascii[i])) {
      return false;
    }
  }
  return true;
}

inline bool HasKnownTag(const uint8_t* p, size_t length) {
  static constexpr std::array<std::string_view, 6> tags{
      "<a_img>", "<b_img>", "<c_img>", "<d_img>",
      "<x_img>", "<y_img>"};
  for (size_t i = 0; i + 14 <= length; i += 2) {
    for (auto tag : tags) {
      bool match = true;
      for (size_t t = 0; t < tag.size(); ++t)
        if (p[i + t * 2] != 0 || p[i + t * 2 + 1] != tag[t]) {
          match = false;
          break;
        }
      if (match) return true;
    }
  }
  return false;
}

inline size_t KnownTagCount(std::u16string_view text) {
  static constexpr std::array<std::u16string_view, 6> tags{
      u"<a_img>", u"<b_img>", u"<c_img>", u"<d_img>",
      u"<x_img>", u"<y_img>"};
  size_t count = 0;
  for (size_t i = 0; i < text.size();) {
    bool matched = false;
    for (auto tag : tags) {
      if (text.substr(i, tag.size()) == tag) {
        ++count;
        i += tag.size();
        matched = true;
        break;
      }
    }
    if (!matched) ++i;
  }
  return count;
}

inline bool FirstKnownTagLayout(std::u16string_view text,
                                uint32_t& element_ordinal,
                                uint32_t& action_index) {
  static constexpr std::array<std::u16string_view, 6> tags{
      u"<a_img>", u"<b_img>", u"<c_img>", u"<d_img>",
      u"<x_img>", u"<y_img>"};
  // Matches Rewrite(): a/b/c/d are A/B/X/Y, x/y are X/Y aliases.
  static constexpr std::array<uint32_t, 6> actions{0, 1, 2, 3, 2, 3};

  uint32_t element = 0;
  for (size_t i = 0; i < text.size();) {
    bool matched = false;
    for (size_t k = 0; k < tags.size(); ++k) {
      const auto tag = tags[k];
      if (text.substr(i, tag.size()) != tag) continue;
      element_ordinal = element;
      action_index = actions[k];
      return true;
    }
    // The scanner accepts only BMP text units here, so each ordinary UTF-16
    // code unit maps to one laid-out glyph element.
    ++element;
    ++i;
  }
  return false;
}

// Walk the committed front-end heap, locating NUL-terminated UTF-16BE source
// strings containing known inline tags. The title prompt and main menu strings
// were observed in 0x40000000..0x44000000. No asset or code pages are touched.
inline void Scan(uint8_t* arena, uintptr_t kStart, uintptr_t kEnd,
                 const ActionLabels& actions, std::vector<Patched>& patches,
                 uint64_t& matches, uint64_t& replacements) {
  constexpr size_t kMaxChars = 160;
  const KeyLabels labels{
      actions[0], actions[1], actions[2], actions[3]};
  const uintptr_t base = reinterpret_cast<uintptr_t>(arena);
  for (uintptr_t offset = kStart; offset < kEnd;) {
    MEMORY_BASIC_INFORMATION m{};
    const auto addr = reinterpret_cast<const void*>(base + offset);
    if (!VirtualQuery(addr, &m, sizeof m)) {
      offset += 0x1000u;
      continue;
    }
    const uintptr_t region_end = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
    if (region_end <= base + offset) break;
    const uintptr_t next = std::min(region_end - base, kEnd);
    if (m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) == 0 ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
      offset = next;
      continue;
    }
    uint8_t* data = arena + offset;
    const size_t length = static_cast<size_t>(next - offset);
    for (size_t i = 0; i + 16 < length; i += 2) {
      if (data[i] != 0) continue;
      const bool tag_candidate =
          data[i + 1] == '<' &&
          HasKnownTag(data + i, std::min(size_t(14), length - i));
      const size_t remaining = length - i;
      const bool instruction_candidate =
          BeAsciiAt(data + i, remaining, "Press ") ||
          BeAsciiAt(data + i, remaining, "Hold ") ||
          BeAsciiAt(data + i, remaining, "Tap ") ||
          BeAsciiAt(data + i, remaining, "Select ");
      if (!tag_candidate && !instruction_candidate) continue;

      size_t start = i;
      while (start >= 2 && i - start < kMaxChars * 2 &&
             TextUnit(Be16(data + start - 2))) start -= 2;
      // Recover the whole bounded source first. A known NUL boundary is
      // required for ordinary strings. The title allocation historically
      // appears without one in some heap layouts, so a leading "Press " is
      // provisionally allowed and validated as title markup below.
      const bool starts_press =
          Be16(data + start) == 'P' && Be16(data + start + 2) == 'r' &&
          Be16(data + start + 4) == 'e' && Be16(data + start + 6) == 's' &&
          Be16(data + start + 8) == 's' && Be16(data + start + 10) == ' ';
      if (i - start >= kMaxChars * 2) continue;
      const bool nul_boundary = start >= 2 && Be16(data + start - 2) == 0;
      if (!starts_press && !nul_boundary) continue;
      size_t end = start;
      std::u16string text;
      for (; end + 1 < length && text.size() < kMaxChars; end += 2) {
        const uint16_t c = Be16(data + end);
        if (c == 0) break;
        if (!TextUnit(c)) break;
        text.push_back(static_cast<char16_t>(c));
      }
      if (end + 1 >= length || Be16(data + end) != 0 || end <= i) continue;
      const bool has_tag = HasKnownTag(data + start, end - start);
      const bool title = starts_press && has_tag;
      if (!title && !nul_boundary) continue;

      const auto tag_rewritten = Rewrite(text, labels);
      const auto rewritten = RewriteFixedInstructions(tag_rewritten, actions);
      if (rewritten == text) {
        i = end;
        continue;
      }

      ++matches;
      if (title) {
        const size_t tag_count = KnownTagCount(text);
        if (tag_count != 0 && text.size() >= tag_count * 6) {
          // Each seven-character <x_img> tag is one laid-out sprite in the
          // stock prompt, but seven ordinary glyphs after Rewrite().
          TitleExpectedBakedElements().store(
              static_cast<uint32_t>(text.size() - tag_count * 6),
              std::memory_order_release);
          TitleExpectedKeyboardElements().store(
              static_cast<uint32_t>(text.size()),
              std::memory_order_release);

          uint32_t icon_element = UINT32_MAX;
          uint32_t icon_action = UINT32_MAX;
          if (FirstKnownTagLayout(text, icon_element, icon_action)) {
            TitleExpectedIconElement().store(icon_element,
                                             std::memory_order_release);
            TitleExpectedIconAction().store(icon_action,
                                            std::memory_order_release);
          }
        }
      }
      // Guest allocations and cached lengths are never resized here. Every
      // source-layer rewrite must preserve the exact UTF-16 code-unit count;
      // reject future rewrite rules that would shorten as well as grow it.
      if (rewritten.size() != text.size()) {
        i = end;
        continue;
      }
      uint8_t* target = data + start;
      const size_t bytes = (text.size() + 1) * 2;
      if (!Writable(target, bytes)) continue;
      std::vector<uint8_t> original(target, target + bytes);
      std::vector<uint8_t> changed(bytes, 0);
      for (size_t j = 0; j < rewritten.size(); ++j) {
        changed[j * 2] = static_cast<uint8_t>(rewritten[j] >> 8);
        changed[j * 2 + 1] = static_cast<uint8_t>(rewritten[j]);
      }
      // Only mutate when the text still matches the sampled original.
      if (std::memcmp(target, original.data(), bytes) != 0) continue;
      std::memcpy(target, changed.data(), bytes);
      patches.push_back(
          {target, std::move(original), std::move(changed), title});
      if (title) TitleSourceActive().store(true, std::memory_order_release);
      ++replacements;
      i = end;
    }
    offset = next;
  }
}

inline ActionLabels CurrentActions() {
  return {
      DisplayKey(rex::cvar::GetFlagByName("keybind_a")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_b")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_x")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_y")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_left_shoulder")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_right_shoulder")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_left_trigger")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_right_trigger")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_start")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_back")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_lstick_press")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_rstick_press")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_dpad_up")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_dpad_down")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_dpad_left")),
      DisplayKey(rex::cvar::GetFlagByName("keybind_dpad_right")),
  };
}

inline KeyLabels FaceKeys(const ActionLabels& actions) {
  return {actions[0], actions[1], actions[2], actions[3]};
}

// Diagnostic log: distinguishes "source replaced" from "visible UI relaid
// out". Seeing replacements in this file is not proof they reached the screen.
inline FILE* LogFile() {
  static FILE* f = [] {
    FILE* out = nullptr;
    fopen_s(&out, "fable2_button_prompts.log", "w");
    return out;
  }();
  return f;
}
inline void Log(const char* message, const std::string& extra = {}) {
  if (FILE* f = LogFile()) {
    std::fprintf(f, "[button-prompts] %s %s\n", message, extra.c_str());
    std::fflush(f);
  }
}
inline std::unique_ptr<std::jthread>& Worker() {
  static std::unique_ptr<std::jthread> worker;
  return worker;
}
inline std::mutex& WorkerMutex() {
  static auto* mutex = new std::mutex;
  return *mutex;
}
inline void Loop(std::stop_token stop, uint8_t* arena) {
  std::vector<Patched> patches;
  ActionLabels previous_actions{};
  bool previous_keyboard = false;
  bool previous_overlay = false;
  bool have_previous = false;
  auto next_title = std::chrono::steady_clock::now();
  auto next_fast = next_title;
  auto next_broad = next_title;
  auto next_report = next_title;
  auto next_gui_sync = next_title;
  constexpr uintptr_t kBroadStart = 0x40000000u;
  constexpr uintptr_t kBroadEnd = 0x44000000u;
  constexpr uintptr_t kBroadSlice = 0x00200000u;  // 2 MiB
  constexpr uintptr_t kBroadOverlap = 320u;       // kMaxChars UTF-16 bytes
  uintptr_t broad_cursor = kBroadStart;
  uint64_t matches = 0, replacements = 0;
  Log("started; native prompt source rewrite: 64KiB title fast path + rolling front-end scan");
  while (!stop.stop_requested()) {
    const auto active = rex::cvar::GetFlagByName("input_prompt_keyboard");
    // A mismatched older SDK lacks the device flag. KeyboardPromptModeActive()
    // treats any value other than explicit "false" as keyboard in auto mode,
    // preserving the conservative fallback used by the original worker.
    const bool keyboard = KeyboardPromptModeActive();
    const bool overlay =
        rex::cvar::GetFlagByName("ui_prompt_overlay_fallback") != "false";
    const auto actions = keyboard ? CurrentActions() : ActionLabels{};
    const auto keys = FaceKeys(actions);
    const bool source_changed =
        !have_previous || keyboard != previous_keyboard ||
        actions != previous_actions;
    const bool graphical_changed =
        source_changed || overlay != previous_overlay;

    if (source_changed) {
      Restore(patches);
      previous_actions = actions;
      previous_keyboard = keyboard;
      next_title = next_fast = next_broad =
          std::chrono::steady_clock::now();
      broad_cursor = kBroadStart;
      Log(keyboard ? "keyboard mode" : "controller mode",
          "input=" + active + " A=" + keys[0] +
          " B=" + keys[1] + " X=" + keys[2] + " Y=" + keys[3]);
    }
    const auto gui_sync_interval =
        keyboard && overlay ? std::chrono::milliseconds(100)
                            : std::chrono::milliseconds(750);
    if (graphical_changed) {
      previous_overlay = overlay;
      have_previous = true;
      SyncGraphicalPrompts(keyboard);
      next_gui_sync = std::chrono::steady_clock::now() + gui_sync_interval;
    }
    if (keyboard) {
      // Reused/freed guest strings may no longer belong to this list. Never
      // restore changed guest contents unless they still match our exact bytes.
      for (auto it = patches.begin(); it != patches.end();)
        it = Same(*it) ? std::next(it) : patches.erase(it);
      RefreshTitleSourceState(patches);
      auto now = std::chrono::steady_clock::now();
      // The title source has moved between runs but every measured allocation
      // stayed inside 0x4266xxxx. Scan just that 64 KiB page range frequently
      // so the native game-font rewrite can beat the first/rebuilt layout
      // without repeatedly walking a full MiB.
      if (now >= next_title) {
        Scan(arena, 0x42660000u, 0x42670000u, actions, patches,
             matches, replacements);
        next_title = now + std::chrono::milliseconds(15);
      }
      // Broader safety net for a future allocator shift within the documented
      // 0x426xxxxx neighborhood. This is intentionally much slower now that
      // the precise title range has its own cheap fast path.
      if (now >= next_fast) {
        Scan(arena, 0x42600000u, 0x42700000u, actions, patches,
             matches, replacements);
        next_fast = now + std::chrono::milliseconds(120);
      }
      // Roll the 64 MiB front-end/localization fallback across small slices
      // instead of walking the whole range in one burst. At 2 MiB every 60 ms
      // a complete pass still takes about two seconds, but render/input threads
      // no longer see a periodic large CPU spike. Adjacent slices overlap by
      // one maximum prompt-string width so a string crossing a slice boundary
      // is never skipped.
      if (now >= next_broad) {
        const uintptr_t logical_end =
            std::min(broad_cursor + kBroadSlice, kBroadEnd);
        const uintptr_t scan_end =
            std::min(logical_end + kBroadOverlap, kBroadEnd);
        Scan(arena, broad_cursor, scan_end, actions, patches,
             matches, replacements);
        broad_cursor =
            logical_end >= kBroadEnd ? kBroadStart : logical_end;
        next_broad = now + std::chrono::milliseconds(60);
      }
    }
    const auto now = std::chrono::steady_clock::now();
    // Presentations are created and destroyed independently of the source
    // strings. In keyboard+overlay mode a lightweight Lua refresh runs around
    // 10 Hz so moving HUD/world prompt keycaps track their element; the Lua
    // side performs full discovery/text adaptation only about every eighth
    // refresh. Other modes keep the older 750 ms full-sync cadence.
    // enqueue_all() coalesces broadcasts by tag.
    if (now >= next_gui_sync) {
      SyncGraphicalPrompts(keyboard);
      next_gui_sync = now + gui_sync_interval;
    }
    if (now >= next_report) {
      Log("scan totals", "known_strings=" + std::to_string(matches) +
          " replaced=" + std::to_string(replacements) +
          " tracked=" + std::to_string(patches.size()));
      next_report = now + std::chrono::seconds(5);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  }
  Restore(patches);
  SyncGraphicalPrompts(false);
  Log("stopped and restored tracked source strings and graphical prompts");
}

inline void OnFrame(uint8_t* arena) {
  if (!arena) return;
  std::lock_guard lock(WorkerMutex());
  if (!Worker()) Worker() = std::make_unique<std::jthread>(
      [arena](std::stop_token stop) { Loop(stop, arena); });
}
inline void Stop() {
  std::unique_ptr<std::jthread> worker;
  {
    std::lock_guard lock(WorkerMutex());
    worker = std::move(Worker());
  }
  if (worker) {
    worker->request_stop();
    worker->join();
  }
}
#else
inline bool TitleSourceRewriteActive() { return false; }
inline uint32_t TitleExpectedBakedElementCount() { return 0; }
inline uint32_t TitleExpectedKeyboardElementCount() { return 0; }
inline void OnFrame(uint8_t*) {}
inline void Stop() {}
#endif
}  // namespace fable2::buttonprompts
