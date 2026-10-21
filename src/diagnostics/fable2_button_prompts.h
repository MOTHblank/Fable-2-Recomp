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
#ifdef _WIN32
struct Patched {
  uint8_t* address;
  std::vector<uint8_t> original;
  std::vector<uint8_t> changed;
};

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

// Only restore bytes if they're still exactly the ones we wrote. The guest can
// free or reuse a heap allocation at any time; never write to stale contents.
inline void Restore(std::vector<Patched>& patches) {
  for (auto& p : patches)
    if (Same(p)) std::memcpy(p.address, p.original.data(), p.original.size());
  patches.clear();
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

// Walk the committed front-end heap, locating NUL-terminated UTF-16BE source
// strings containing known inline tags. The title prompt and main menu strings
// were observed in 0x40000000..0x44000000. No asset or code pages are touched.
inline void Scan(uint8_t* arena, uintptr_t kStart, uintptr_t kEnd,
                 const KeyLabels& labels, std::vector<Patched>& patches,
                 uint64_t& matches, uint64_t& replacements) {
  constexpr size_t kMaxChars = 160;
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
      if (data[i] != 0 || data[i + 1] != '<' ||
          !HasKnownTag(data + i, std::min(size_t(14), length - i))) continue;

      size_t start = i;
      while (start >= 2 && i - start < kMaxChars * 2 &&
             TextUnit(Be16(data + start - 2))) start -= 2;
      // Only edit a bounded string with a known start. In the title screen
      // the source begins with "Press "; elsewhere require a NUL boundary.
      const bool title = i >= start + 12 &&
          Be16(data + start) == 'P' && Be16(data + start + 2) == 'r' &&
          Be16(data + start + 4) == 'e' && Be16(data + start + 6) == 's' &&
          Be16(data + start + 8) == 's' && Be16(data + start + 10) == ' ';
      if (i - start >= kMaxChars * 2 ||
          (!title && (start < 2 || Be16(data + start - 2) != 0))) continue;
      size_t end = start;
      std::u16string text;
      for (; end + 1 < length && text.size() < kMaxChars; end += 2) {
        const uint16_t c = Be16(data + end);
        if (c == 0) break;
        if (!TextUnit(c)) break;
        text.push_back(static_cast<char16_t>(c));
      }
      if (end + 1 >= length || Be16(data + end) != 0 ||
          end <= i || !HasKnownTag(data + start, end - start)) continue;
      ++matches;
      const auto rewritten = Rewrite(text, labels);
      if (rewritten == text || rewritten.size() > text.size()) continue;
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
      patches.push_back({target, std::move(original), std::move(changed)});
      ++replacements;
      i = end;
    }
    offset = next;
  }
}

inline KeyLabels CurrentKeys() {
  return {DisplayKey(rex::cvar::GetFlagByName("keybind_a")),
          DisplayKey(rex::cvar::GetFlagByName("keybind_b")),
          DisplayKey(rex::cvar::GetFlagByName("keybind_x")),
          DisplayKey(rex::cvar::GetFlagByName("keybind_y"))};
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
  KeyLabels previous_keys{};
  bool previous_keyboard = false;
  auto next_fast = std::chrono::steady_clock::now();
  auto next_full = next_fast;
  auto next_report = next_fast;
  uint64_t matches = 0, replacements = 0;
  Log("started; scan is native UI source-string replacement, not a texture hook");
  while (!stop.stop_requested()) {
    const auto mode = rex::cvar::GetFlagByName("ui_prompt_mode");
    const auto active = rex::cvar::GetFlagByName("input_prompt_keyboard");
    // A mismatched older SDK lacks the device flag. Prefer keyboard as the
    // conservative auto fallback instead of silently hiding the feature.
    const bool keyboard = mode == "keyboard" ||
        (mode != "controller" && active != "false");
    const auto keys = keyboard ? CurrentKeys() : KeyLabels{};
    if (keyboard != previous_keyboard || keys != previous_keys) {
      Restore(patches);
      previous_keys = keys;
      previous_keyboard = keyboard;
      next_fast = next_full = std::chrono::steady_clock::now();
      Log(keyboard ? "keyboard mode" : "controller mode",
          "input=" + active + " A=" + keys[0] +
          " B=" + keys[1] + " X=" + keys[2] + " Y=" + keys[3]);
    }
    if (keyboard) {
      // Reused/freed guest strings may no longer belong to this list. Never
      // restore changed guest contents unless they still match our exact bytes.
      for (auto it = patches.begin(); it != patches.end();)
        it = Same(*it) ? std::next(it) : patches.erase(it);
      auto now = std::chrono::steady_clock::now();
      // Fast path for the documented title-prompt allocation neighborhood.
      // At 1 MiB per 40 ms this is bounded and runs only off the render thread.
      if (now >= next_fast) {
        Scan(arena, 0x42600000u, 0x42700000u, keys, patches,
             matches, replacements);
        next_fast = now + std::chrono::milliseconds(40);
      }
      // Other front-end / localized strings are discovered more slowly; this
      // includes menu text and varied translations.
      if (now >= next_full) {
        Scan(arena, 0x40000000u, 0x44000000u, keys, patches,
             matches, replacements);
        next_full = now + std::chrono::seconds(2);
      }
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_report) {
      Log("scan totals", "known_strings=" + std::to_string(matches) +
          " replaced=" + std::to_string(replacements) +
          " tracked=" + std::to_string(patches.size()));
      next_report = now + std::chrono::seconds(5);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  }
  Restore(patches);
  Log("stopped and restored tracked source strings");
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
inline void OnFrame(uint8_t*) {}
inline void Stop() {}
#endif
}  // namespace fable2::buttonprompts
