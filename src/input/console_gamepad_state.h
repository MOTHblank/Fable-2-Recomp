// Shared state for the developer console's synthetic guest gamepad.
//
// Kept separate from fable2_console.h because that header defines CVars and
// commands and therefore must only be included by the owning translation unit.

#pragma once

#include <atomic>

#include "remote_input_state.h"

namespace fable2::console {

inline fable2::remote::InputStateStore& PadStore() {
  static fable2::remote::InputStateStore state;
  return state;
}

inline std::atomic<bool>& PadEnabled() {
  static std::atomic<bool> enabled{true};
  return enabled;
}

}  // namespace fable2::console
