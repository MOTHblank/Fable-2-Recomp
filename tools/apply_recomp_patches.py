#!/usr/bin/env python3
"""Apply recomp-level patches to codegen's output (generated/default/*.cpp).

Recomps run NATIVE code, so a Xenia code-region patch (a guest .text byte
change) has no runtime effect on its own. PREFERRED mechanism: the SDK's
mid-asm hooks ([[entrypoint.midasm_hook]] in fable_2_manifest.toml + a hook
function in src/fable2_hooks.cpp) - see docs/patches.md. This script is the
fallback for patches that cannot be expressed as hooks (e.g. constants
baked into memory operands). It runs as a build step between codegen and
compiling fable_2_recomp (wired in CMakeLists.txt), keyed on
codegen.build.stamp, so patches survive codegen re-runs.

Patches are applied as anchored text replacements with a unique marker
comment (idempotent: already-patched files are left alone). Missing anchors
are a hard error - a silently-skipped patch is worse than a failed build.

Set FABLE2_RECOMP_PATCHES=0 to skip all patches (used for A/B runs).
"""

import os
import sys

GENERATED_DIR = sys.argv[1] if len(sys.argv) > 1 else "generated/default"

# Each patch: (name, anchor, replacement). The anchor must occur exactly once
# per generated file that contains the target function. The replacement ends
# with the patch's marker comment, which is the idempotency check.
# Patches that cannot be expressed as a [[entrypoint.midasm_hook]] (see
# docs/patches.md).
PATCHES = []

MARKER = "// [recomp-patch: "

DIRECT_MOUSE_NAME = "direct-mouse-native-camera-input"
DIRECT_MOUSE_FUNCTION = "DEFINE_REX_FUNC(ProcessAndProcessAndProcess1253_832BB018)"
DIRECT_MOUSE_STATE_ANCHOR = "ctx.r10.s64 = ctx.r1.s64 + 708;"


def _patch_direct_mouse_native_camera(files) -> int:
    """Inject raw mouse deltas into the local XINPUT_STATE consumed by the camera.

    0x832BB018 is called from the per-frame 0x832BD3E8 input dispatcher and
    builds a local XINPUT_STATE at r1+708.  The camera path later reads
    sThumbRX/sThumbRY from +12/+14.  Patching that local copy (rather than the
    finished camera transform) lets the original camera code retain collision,
    obstruction handling, authored framing, lock-on behavior and native clamps.
    """
    marker = MARKER + DIRECT_MOUSE_NAME + "]"
    found_function = False
    for fname in files:
        path = os.path.join(GENERATED_DIR, fname)
        with open(path, "rb") as f:
            data = f.read().decode("utf-8")
        function_start = data.find(DIRECT_MOUSE_FUNCTION)
        if function_start < 0:
            continue
        found_function = True

        next_function = data.find("\nDEFINE_REX_FUNC(", function_start + len(DIRECT_MOUSE_FUNCTION))
        function_end = len(data) if next_function < 0 else next_function
        body = data[function_start:function_end]

        if marker in body:
            return 0

        anchor_count = body.count(DIRECT_MOUSE_STATE_ANCHOR)
        if anchor_count != 1:
            raise RuntimeError(
                f"{DIRECT_MOUSE_FUNCTION}: expected one local XINPUT_STATE anchor "
                f"({DIRECT_MOUSE_STATE_ANCHOR!r}), found {anchor_count}"
            )

        # Keep the declarations in the same TU as the generated function.
        # Key this on our cvar declaration rather than the header include so a
        # future codegen/PCH change cannot leave the injected body undeclared.
        include_anchor = '#include "fable_2_pch.h"\n'
        declaration_marker = 'REXCVAR_DECLARE(bool, mouse_direct_look);'
        declarations = (
            '#include "fable_2_pch.h"\n'
            '#include "mouse_look.h"\n'
            '#include <algorithm>\n'
            '#include <cmath>\n'
            '#include <rex/cvar.h>\n\n'
            'REXCVAR_DECLARE(bool, mouse_direct_look);\n'
            'REXCVAR_DECLARE(bool, mouse_look);\n'
            'REXCVAR_DECLARE(int32_t, mouse_look_scale);\n'
            'REXCVAR_DECLARE(int32_t, mouse_look_scale_y);\n'
            'REXCVAR_DECLARE(bool, mouse_invert_y);\n'
        )
        if declaration_marker not in data:
            if include_anchor not in data:
                raise RuntimeError(f"{fname}: generated PCH include anchor missing")
            data = data.replace(include_anchor, declarations, 1)
            # Recompute the function slice after the include insertion.
            function_start = data.find(DIRECT_MOUSE_FUNCTION)
            next_function = data.find("\nDEFINE_REX_FUNC(",
                                      function_start + len(DIRECT_MOUSE_FUNCTION))
            function_end = len(data) if next_function < 0 else next_function
            body = data[function_start:function_end]

        injection = DIRECT_MOUSE_STATE_ANCHOR + r'''
	// [recomp-patch: direct-mouse-native-camera-input]
	// The SDK only sends motion to the raw side-channel while the game-side
	// camera classifier says direct input is safe.  Modify this function's
	// local XINPUT_STATE, never the device state itself, so physical stick
	// input remains additive and every downstream camera rule stays native.
	if (REXCVAR_GET(mouse_direct_look) && REXCVAR_GET(mouse_look) &&
	    REX_LOAD_U32(ctx.r10.u32 + 0) != 0) {
		float mouse_dx = 0.0f;
		float mouse_dy = 0.0f;
		fable2::mouselook::take_delta(mouse_dx, mouse_dy);
		if (std::isfinite(mouse_dx) && std::isfinite(mouse_dy)) {
			const int32_t scale_x = REXCVAR_GET(mouse_look_scale);
			const int32_t scale_y = REXCVAR_GET(mouse_look_scale_y);
			if (scale_x >= 1 && scale_x <= 4096 &&
			    scale_y >= 1 && scale_y <= 4096) {
				auto add_axis = [](uint16_t raw, float pixels,
				                   int32_t units_per_pixel) -> uint16_t {
					const int32_t native_axis =
					    static_cast<int32_t>(static_cast<int16_t>(raw));
					const double scaled = std::clamp(
					    double(pixels) * double(units_per_pixel),
					    -32767.0, 32767.0);
					const int32_t mouse_axis =
					    static_cast<int32_t>(std::lround(scaled));
					const int32_t combined = std::clamp(
					    native_axis + mouse_axis, -32768, 32767);
					return static_cast<uint16_t>(
					    static_cast<int16_t>(combined));
				};
				const uint16_t native_rx = REX_LOAD_U16(ctx.r10.u32 + 12);
				const uint16_t native_ry = REX_LOAD_U16(ctx.r10.u32 + 14);
				const int32_t y_scale =
				    REXCVAR_GET(mouse_invert_y) ? scale_y : -scale_y;
				REX_STORE_U16(ctx.r10.u32 + 12,
				    add_axis(native_rx, mouse_dx, scale_x));
				REX_STORE_U16(ctx.r10.u32 + 14,
				    add_axis(native_ry, mouse_dy, y_scale));
			}
		} else {
			fable2::mouselook::request_reset();
		}
	}
'''
        patched_body = body.replace(DIRECT_MOUSE_STATE_ANCHOR, injection, 1)
        data = data[:function_start] + patched_body + data[function_end:]
        with open(path, "wb") as f:
            f.write(data.encode("utf-8"))
        print(f"[recomp-patches] applied '{DIRECT_MOUSE_NAME}' in {fname}")
        return 1

    if not found_function:
        raise RuntimeError(
            f"camera input function {DIRECT_MOUSE_FUNCTION!r} not found in "
            f"{len(files)} generated file(s)"
        )
    return 0


def main() -> int:
    if os.environ.get("FABLE2_RECOMP_PATCHES") == "0":
        print("[recomp-patches] FABLE2_RECOMP_PATCHES=0 - skipping")
        return 0

    files = sorted(
        f for f in os.listdir(GENERATED_DIR) if f.startswith("fable_2_recomp.") and f.endswith(".cpp")
    )
    if not files:
        print(f"[recomp-patches] ERROR: no fable_2_recomp.*.cpp in {GENERATED_DIR}", file=sys.stderr)
        return 1

    applied_total = 0
    try:
        applied_total += _patch_direct_mouse_native_camera(files)
    except RuntimeError as exc:
        print(f"[recomp-patches] ERROR: {exc}", file=sys.stderr)
        return 1

    for name, anchor, replacement in PATCHES:
        marker = MARKER + name + "]"  # MARKER ends with a space
        found = False
        for fname in files:
            path = os.path.join(GENERATED_DIR, fname)
            with open(path, "rb") as f:
                data = f.read().decode("utf-8")
            if marker in data:
                found = True  # already applied (idempotent)
                continue
            if anchor not in data:
                continue
            data = data.replace(anchor, replacement, 1)
            with open(path, "wb") as f:
                f.write(data.encode("utf-8"))
            found = True
            print(f"[recomp-patches] applied '{name}' in {fname}")
            applied_total += 1
        if not found:
            print(f"[recomp-patches] ERROR: anchor for '{name}' not found in any "
                  f"{len(files)} file(s) under {GENERATED_DIR}", file=sys.stderr)
            return 1

    print(f"[recomp-patches] done ({applied_total} applied now, rest already present)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
