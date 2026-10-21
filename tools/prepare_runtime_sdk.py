"""Apply the source-only runtime fixes to a known-good SDK revision.

No game content is read. Conflicting SDK edits are rejected, not overwritten.
Use --skip-dependencies for patch validation without network/dependency setup.

Accepted SDK states:
  * HEAD at the base pin - a clean SDK checkout; the patches under
    thirdparty/ are applied on top of it, and
  * the repository's explicitly pinned fork with the integrated runtime and
    debug fixes verified from working-tree source files, or
  * any other revision where both original patches reverse-apply cleanly.
"""
import argparse
from pathlib import Path
import subprocess
import sys

# Clean base revision the patches were generated against.
SDK_PIN = "babc769a94be5618010abfd075ed84f3c2bc09f5"
# The repository's current SDK submodule pin integrates the original runtime
# fixes, but vendors libmspack as ordinary files. The old runtime patch
# replaces a *gitlink*, so "git apply --reverse --check" cannot verify the
# whole patch on this fork even when the code is present.
BAKED_SDK_PIN = "92ddff153f98b9bdd385bbf72cf0dca9a7cda4b3"

# Verify every area affected by the original runtime-fixes patch and the
# dependent debug-exports patch. Check the actual working files, not just HEAD:
# an edited/incomplete SDK checkout must not be treated as fixed.
BAKED_RUNTIME_MARKERS = {
    "include/rex/audio/clocked_audio_sink.h": (
        "class ClockedAudioSink",
        "bool Start()",
        "bool Submit()",
        "void Stop()",
    ),
    "include/rex/audio/sdl/sdl_audio_driver.h": (
        "#include <rex/audio/clocked_audio_sink.h>",
        "bool InitializeDevice()",
        "std::unique_ptr<ClockedAudioSink> silent_sink_",
    ),
    "include/rex/graphics/command_processor.h": (
        "#include <rex/graphics/frame_limiter.h>",
        "#include <rex/graphics/guest_frame_meter.h>",
        "FrameLimiter frame_limiter_",
        "GuestFrameMeter guest_frame_meter_",
    ),
    "include/rex/graphics/frame_limiter.h": ("class FrameLimiter", "void Pace(int frames_per_second)"),
    "include/rex/graphics/guest_frame_meter.h": ("class GuestFrameMeter", "Record(Clock::time_point now)"),
    "src/audio/sdl/sdl_audio_driver.cpp": (
        "if (InitializeDevice()) return true;",
        "std::make_unique<ClockedAudioSink>",
        "if (silent_sink_)",
        "silent_sink_->Stop()",
    ),
    "src/graphics/CMakeLists.txt": ("frame_limiter.cpp",),
    "src/graphics/command_processor.cpp": (
        "REXCVAR_DEFINE_INT32(frame_limit",
        "frame_limiter_.Pace(REXCVAR_GET(frame_limit))",
        "guest_frame_meter_.Record",
        "rex::perf::SetCounter",
        "rex::perf::ResetFrameCounters",
    ),
    "src/graphics/frame_limiter.cpp": (
        "FrameLimiter::~FrameLimiter()",
        "FrameLimiter::WaitUntil",
    ),
    "src/graphics/graphics_system.cpp": (
        "REXCVAR_DEFINE_BOOL(guest_vblank_pacing",
        "REXCVAR_GET(guest_vblank_pacing)",
    ),
    "src/system/CMakeLists.txt": (
        "list(FILTER REXRUNTIME_EXPORT_LINES",
        "rexruntime-d3d12.def",
    ),
    "src/system/rexruntime.def": (
        "?SetCounter@perf@rex",
        "?ResetFrameCounters@perf@rex",
        "?SetIcon@Window@ui@rex",
        "?IncrementCounter@perf@rex",
        "?WriteCsvFrame@perf@rex",
        "?FlushCsv@perf@rex",
        "?GetFlagByName@cvar@rex",
        "?GetRegistry@cvar@rex",
    ),
}

# Public libmspack commit the SDK's broken pin is repointed at.
MSPACK_PIN = "305907723a4e7ab2018e58040059ffb5e77db837"
PATCHES = (
    "rexglue-sdk-runtime-fixes.patch",
    "rexglue-sdk-debug-exports.patch",
)


def fail(message):
    print(f"prepare_runtime_sdk: error: {message}", file=sys.stderr)
    raise SystemExit(1)


def git(source, *args, check=True):
    result = subprocess.run(["git", "-C", str(source), *args],
                            capture_output=True, text=True)
    if check and result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        fail(f"git {' '.join(args)} failed (exit {result.returncode}):\n{detail}")
    return result


def patch_state(source, patch):
    """Classify the worktree as 'applied', 'clean', or 'conflict' for a patch."""
    if git(source, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
        return "applied"
    if git(source, "apply", "--check", str(patch), check=False).returncode == 0:
        return "clean"
    return "conflict"


def missing_baked_runtime_features(source):
    """Return an actionable list of missing features from the known SDK fork."""
    missing = []
    for relative, markers in BAKED_RUNTIME_MARKERS.items():
        path = source / relative
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError):
            missing.append(f"{relative} (missing or unreadable)")
            continue
        for marker in markers:
            if marker not in content:
                missing.append(f"{relative}: {marker}")

    # The runtime-fixes patch also changes the libmspack gitlink. The
    # integrated fork has the dependency vendored instead of a submodule.
    if libmspack_mode(source) != "040000":
        missing.append("thirdparty/libmspack (expected vendored SDK sources)")
    return missing


def libmspack_mode(source):
    """Return the mode of thirdparty/libmspack at HEAD ('160000' submodule
    gitlink, '040000' vendored plain files), or None if absent."""
    result = git(source, "ls-tree", "HEAD", "--", "thirdparty/libmspack", check=False)
    if result.returncode != 0 or not result.stdout.strip():
        return None
    return result.stdout.split()[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--skip-dependencies", action="store_true")
    args = parser.parse_args()
    source = args.source.resolve()
    patch_directory = Path(__file__).resolve().parents[1] / "thirdparty"

    if not source.is_dir():
        fail(f"SDK source directory does not exist: {source}")
    if git(source, "rev-parse", "--is-inside-work-tree", check=False).returncode != 0:
        fail(f"{source} is not a git work tree; run "
             "'git submodule update --init thirdparty/rexglue-sdk' from the repo root.")

    head = git(source, "rev-parse", "HEAD", check=False).stdout.strip()
    if not head:
        fail(f"{source} has no commits (empty SDK checkout).")

    # Keep follow-up fixes separate so existing patched SDK checkouts can upgrade.
    states = {}
    for name in PATCHES:
        patch = patch_directory / name
        if not patch.is_file():
            fail(f"patch file is missing: {patch}")
        states[name] = patch_state(source, patch)

    if head == SDK_PIN:
        # Base revision: apply whatever is missing, refuse conflicting edits.
        for name, state in states.items():
            if state == "applied":
                print(f"Already applied: {name}")
            elif state == "clean":
                git(source, "apply", str(patch_directory / name))
                print(f"Applied: {name}")
            else:
                detail = git(source, "apply", "--check",
                             str(patch_directory / name), check=False).stderr.strip()
                fail(f"SDK patch {name} conflicts with local edits:\n{detail}\n"
                     "The SDK work tree is not at a clean accepted revision. Restore it with:\n"
                     f"  git -C {source} checkout -- . && git -C {source} clean -fd")
    elif head == BAKED_SDK_PIN:
        # This exact fork integrates both original patches, but differs from
        # their line-by-line history (including vendored vs gitlink libmspack).
        # Refuse incomplete/locally modified builds instead of bypassing the
        # patch check for every arbitrary revision.
        missing = missing_baked_runtime_features(source)
        if missing:
            fail("the pinned SDK fork is missing expected runtime fixes:\n  "
                 + "\n  ".join(missing)
                 + "\nRestore the missing SDK files before building; no "
                   "automatic reset or checkout was performed.")
        print(f"SDK revision {head} has the integrated runtime and debug fixes (verified).")
    else:
        # Unknown revisions still require both patches to reverse-apply;
        # never accept an unverified SDK tree merely because it has a fork SHA.
        pending = [name for name, state in states.items() if state != "applied"]
        if pending:
            fail(f"unexpected SDK revision {head} with unapplied patches: "
                 f"{', '.join(pending)}.\n"
                 f"Either check out the base revision ({SDK_PIN}) so the patches "
                 f"can be applied, or use a fork revision that bakes them in.\n"
                 f"Restore the base with: git -C {source} checkout {SDK_PIN}")
        print(f"SDK revision {head} already carries all runtime fixes (baked in).")

    # Keep follow-up fixes idempotent on the pinned checkout and on approved
    # SDK forks which already bake in the original runtime fixes.
    for followup_name in (
        "rexglue-sdk-pipeline-warmup.patch",
        "rexglue-sdk-fable2-performance.patch",
        "rexglue-sdk-mnk-input.patch",
        "rexglue-sdk-adaptive-prompts.patch",
    ):
        followup_patch = patch_directory / followup_name
        if not followup_patch.is_file():
            fail(f"SDK patch file is missing: {followup_patch}")
        followup_state = patch_state(source, followup_patch)
        if followup_state == "clean":
            git(source, "apply", str(followup_patch))
            print(f"Applied: {followup_name}")
        elif followup_state == "applied":
            print(f"Already applied: {followup_name}")
        else:
            # Git apply cannot represent partial patch state or tolerate
            # unrelated nearby edits. Apply only full, exact before/after hunks
            # and refuse any unknown contents without cleaning the SDK tree.
            from apply_sdk_followup_patch import apply_exact_hunks
            try:
                action = apply_exact_hunks(source, followup_patch)
            except (ValueError, OSError) as exc:
                detail = git(source, "apply", "--check", str(followup_patch),
                             check=False).stderr.strip()
                fail(f"SDK patch {followup_name} conflicts with local edits:\n"
                     f"{detail}\nStrict recovery refused: {exc}")
            print(f"{'Recovered' if action == 'applied' else 'Already applied'}: {followup_name}")

    if not args.skip_dependencies:
        # libmspack is either a submodule gitlink (base pin and older fork
        # commits) or plain vendored files (newer fork commits). Only the
        # gitlink form needs the index repin + submodule fetch + Windows
        # symlink materialization; the original SDK pin for it is unavailable
        # on its public remote.
        mode = libmspack_mode(source)
        if mode == "160000":
            # git submodule update reads the index, not the patched worktree gitlink.
            git(source, "update-index", "--cacheinfo",
                f"160000,{MSPACK_PIN},thirdparty/libmspack")
        elif mode != "040000":
            fail("thirdparty/libmspack is missing from the SDK tree at HEAD "
                 "(expected a submodule gitlink or vendored files).")
        result = subprocess.run(["git", "-C", str(source), "submodule", "update",
                                 "--init", "--recursive"])
        if result.returncode != 0:
            fail("SDK submodule update failed; a fresh clone needs network "
                 "access to fetch the SDK submodules.")
        if mode == "160000":
            result = subprocess.run([sys.executable,
                                     str(Path(__file__).with_name("prepare_renderer_mspack.py")),
                                     str(source / "thirdparty" / "libmspack")])
            if result.returncode != 0:
                fail("prepare_renderer_mspack.py failed.")
        else:
            # The SDK's thirdparty/CMakeLists.txt treats a dependency as
            # initialized when <dep>/.git exists. Vendored libmspack has no
            # .git, so drop an empty marker directory (invisible to git
            # status) to satisfy that check.
            marker = source / "thirdparty" / "libmspack" / ".git"
            if not marker.exists():
                marker.mkdir(parents=True, exist_ok=True)


if __name__ == "__main__":
    main()
