// fable_2 - ReXGlue Recompiled Project

#include "generated/default/fable_2_init.h"

// F5 polling is independent of the input driver (MainRenderLoop hook).
#include "fable2_f5_lua.h"

#include "fable_2_app.h"
#include "fable2_button_prompts.h"
#include "fps_meter.h"
#include "fable2_text_probe.h"
#include "fable2_glyph_probe.h"
#include "fable2_hotfuncs.h"
#include "fable2_heap_scan.h"
#include "fable2_text_append.h"
#include "fable2_state_probe.h"
#include "fable2_menu_probe.h"  // main-menu input path (mod-menu hook points)
#include "fable2_modmenu.h"  // 6th "MOD OPTION" selectable main-menu item
#include "fable2_av_probe.h"  // self-installs a guest-PC AV logger (VEH)
#include "fable2_stall_dump.h"  // self-installs a render-stall thread dumper
// #include "fable2_ui_render_probe.h"
#include "keyboard_gamepad.h"

#include <rex/cvar.h>

// Native guest UI button markup: automatic last-active device, or a manual
// override when the user wants only keyboard or only controller prompts.
REXCVAR_DEFINE_STRING(ui_prompt_mode, "auto", "Input",
                     "Prompt display mode: auto, keyboard, controller")
    .allowed({"auto", "keyboard", "controller"});

// Host keyboard -> guest gamepad map (see src/input/keyboard_gamepad.h).
// Format: "Key:Button,Key:Button,...".
// The default layout is NOT baked in here: it lives in the config layer
// (fable2::config::kDefaultKeyboardGamepadMap) and is editable in
// fable2_config.toml under [input]. Fable2App::OnPostInitLogging seeds this
// cvar from the config at startup (only when no higher-priority source set
// it). Override at runtime with --keyboard_gamepad_map "..." or in the
// console; empty string disables the keyboard gamepad.
REXCVAR_DEFINE_STRING(
    keyboard_gamepad_map,
    "",
    "Input",
    "Map host keyboard keys / mouse buttons to guest gamepad input "
    "(Key:Button,...; key names: keyboard keys plus LMB/RMB/MMB and "
    "XMB1/XMB2 mouse buttons; targets: A/B/X/Y, LB/RB, LT/RT, "
    "Up/Down/Left/Right, Pause, Select, L3/R3, StickUp/StickDown/"
    "StickLeft/StickRight). "
    "Default comes from fable2_config.toml [input] keyboard_gamepad_map.");

// Mouse -> right stick through the SDK's SDL3 relative-input driver.
// The defaults below are a fallback only: Fable2App::OnPostInitLogging
// seeds these cvars from fable2_config.toml [input] at startup (when no
// higher-priority source set them). Keep the values in sync with the
// defaults in fable2::config::Values (src/core/fable2_config.h).
REXCVAR_DEFINE_BOOL(mouse_direct_look, false, "Input",
                    "Experimental direct hero-camera orbit from raw mouse motion");
REXCVAR_DEFINE_BOOL(mouse_look, true, "Input",
                    "Map mouse movement to the guest right stick (camera "
                    "look): sweep to look, stop to stop. Default comes from "
                    "fable2_config.toml [input] mouse_look.");
REXCVAR_DEFINE_INT32(mouse_look_scale, 256, "Input",
                     "Mouse-look sensitivity: right-stick units per pixel of "
                     "mouse movement (larger = more sensitive). Default comes "
                     "from fable2_config.toml [input] mouse_look_scale.")
    .range(1, 4096);
REXCVAR_DEFINE_INT32(mouse_look_scale_y, 256, "Input",
                     "Vertical mouse sensitivity in right-stick units per pixel")
    .range(1, 4096);
REXCVAR_DEFINE_BOOL(mouse_invert_y, false, "Input", "Invert mouse look vertically");
REXCVAR_DEFINE_INT32(mouse_deadzone_compensation, 0, "Input",
                     "Additional right-stick threshold compensation for mouse motion")
    .range(0, 12000);
REXCVAR_DEFINE_INT32(mouse_flick_buffer_polls, 4, "Input",
                     "Extra guest input polls to retain clipped mouse flicks (0..12)")
    .range(0, 12);

// The SDK manages cursor capture using focus and active UI state.
// Reserve the debug menu key from gameplay bindings.
REXCVAR_DEFINE_STRING(
    mouse_unlock_key,
    "F4",
    "Input",
    "Host key reserved from the synthetic gamepad map for the debug menu.");

// Skip the default.xex SHA-256 integrity check at startup (see
// Fable2App::OnLoadXexImage). When set, the XEX hash is still computed and
// written to the log, but a mismatch (or an unreadable file) no longer blocks
// the launch. Useful for running a modified or non-catalogue default.xex.
REXCVAR_DEFINE_BOOL(skipHash, false, "Launch",
                    "Skip the default.xex SHA-256 integrity check. The hash is "
                    "still logged, but a mismatch no longer blocks the launch "
                    "(useful for a modified or non-catalogue XEX). Pass "
                    "--skipHash on the command line.");

#ifdef _WIN32
// Hybrid-graphics laptops (Intel/AMD iGPU + NVIDIA/AMD dGPU): ask the drivers
// to run us on the discrete GPU. Without this Windows may hand the game the
// integrated GPU, which hits D3D12 DEVICE_HUNG (TDR) shortly after boot
// (issue #42). These must be exported from the .exe itself; a user's own
// per-app setting in Windows Graphics settings / NVIDIA Control Panel still
// takes priority.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

REX_DEFINE_APP(fable_2, Fable2App::Create)
