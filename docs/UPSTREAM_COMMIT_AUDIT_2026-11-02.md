# Upstream Fable-2-Recomp commit audit — 2026-11-02

Compared `MOTHblank/Fable-2-Recomp:master` with
`himdo/Fable-2-Recomp:master` (upstream head `4d86f4a10d61f9f0e105b836a1e6587520e8c191`).
The initial comparison reported **45 upstream-only commits**, of which
**18 are merge commits**. The counts are graph ancestry, **not a count of
remaining missing features**, because fixes were ported as independent commits.

This document records source-level disposition. It does **not** certify a
successful Windows build or game runtime test. Fork-specific launcher,
disc extraction, camera and renderer architecture were evaluated separately
rather than blindly replaced by upstream's older versions.

Legend: **Ported** = implemented/adapted in fork; **Present** = fork already
has equivalent behavior; **Retained** = fork implementation deliberately kept;
**Deferred** = further runtime compatibility work needed; **Skipped** =
not useful or problematic; **Merge** = duplicate integration of an earlier
commit, no distinct implementation needed.

| # | Upstream SHA | Status | Disposition |
|---:|---|---|---|
| 1 | `bc66c30b24` | Retained | LookStickProcess cleanup concerns an upstream camera path; preserve fork-specific camera handling |
| 2 | `a16eb775e4` | Retained | More of the same LookStickProcess refactor; upstream later described that handler as inactive in normal gameplay |
| 3 | `ea39ab8841` | Partial | Optimized tracing ported, FPS caller adapted, new hotfunc overrides ported; retain separate camera logic and reject invalid wait-pause implementation |
| 4 | `a4b0274e67` | Ported | Second-controller black-menu fix included in the MOTHblank SDK fork, with root-cause documentation |
| 5 | `020607d66b` | Ported | Native guest memory functions and zlib-ng inflate with original-code fallbacks |
| 6 | `f379bdd235` | Merge | Merge of native memory/zlib work from #5 |
| 7 | `1cef3bc145` | Skipped | GitHub Actions CI workflow, deliberately not included in this fork |
| 8 | `6793705c34` | Ported | STFS extraction path traversal containment |
| 9 | `0390f59205` | Present | Staged TOML already equals the embedded template; CI-style source test intentionally not added |
| 10 | `a62445237a` | Ported | Correct root-relative script/image ignore rules |
| 11 | `6e13827791` | Retained | Fork uses a different one-folder disc extraction/build workflow; README maintained for fork |
| 12 | `f61e348d4c` | Ported | zlib-ng pinned to upstream's exact 2.2.4 commit |
| 13 | `ec9d515ca5` | Ported | Private vulnerability-reporting policy in SECURITY.md |
| 14 | `2001598089` | Ported | SHA-256 validation of the downloaded official SDK archive |
| 15 | `8cc6280a6c` | Skipped | Cosmetic SDK success-message wording only |
| 16 | `59cca591dc` | Retained | Old SDK source-setup wording doesn't match the fork's synchronized submodule workflow |
| 17 | `55e38c4518` | Ported | Build/package command adapted for `out/game`, with allowlisted ZIP inputs |
| 18 | `564be2c11f` | Merge | Merge of #7 (CI skipped) |
| 19 | `9a50a06064` | Merge | Merge of #8 (STFS protection ported) |
| 20 | `f9c5a6ab81` | Merge | Merge of #9 (template already synchronized) |
| 21 | `796ed492d4` | Merge | Merge of #14 (SDK hash verification ported) |
| 22 | `2f1cb0b3fb` | Merge | Merge of #12 (zlib pin ported) |
| 23 | `a914cb0e8c` | Merge | Merge of #15 (cosmetic only) |
| 24 | `5c1f0ba68a` | Merge | Merge of #17 (packaging adapted) |
| 25 | `9083c7bc96` | Merge | Merge of #13 (security policy ported) |
| 26 | `833532c0d3` | Ported | Collision checker previously integrated; missing hotfunc generator now restored with safer non-overwriting default |
| 27 | `fcc1055eb3` | Merge | Preparatory merge for script ignore change |
| 28 | `a40f17b6e2` | Merge | Merge of #10 (ignore rules ported) |
| 29 | `771392ae1f` | Skipped | Community Discord link is optional upstream community information |
| 30 | `2df236e751` | Ported | Documentation points to `MOTHblank/rexglue-sdk`; old SDK setup no longer applies |
| 31 | `d8ff7c025d` | Ported | GPU progress/interpolation overrides use explicit declarations or PCH instead of numbered generated function headers |
| 32 | `d38b304d24` | Present | Fork's `build.cmd` already performs first-run codegen |
| 33 | `831f986874` | Ported | `pefile` presence checked during CMake configuration |
| 34 | `026b9577cd` | Ported | Player quickstart adapted to the fork's launcher/extraction and packaging workflow |
| 35 | `9c655a89eb` | Present | Fork does not instruct players to use the removed hidden Choose Game Folder button |
| 36 | `a5ba642222` | Ported | Packager excludes retail `dir.manifest` |
| 37 | `3ab0db50ac` | Merge | Merge of #32 (already implemented) |
| 38 | `55a2e4c5a6` | Merge | Merge of #34 (quickstart adapted) |
| 39 | `3955439f84` | Merge | Merge of #36 (manifest excluded) |
| 40 | `4e1e4c0dff` | Ported | Ordinary Release preset enables PE cleaning; profiling opt-out preserved |
| 41 | `a453b9b730` | Ported | README explains `pefile` is needed by PE cleaning |
| 42 | `49ce7876bd` | Merge | Merge of #11 (fork-specific docs retained) |
| 43 | `148ecb8d6d` | Merge | Merge of #31 (partition-independent override files now ported) |
| 44 | `b08ddedbc9` | Merge | Merge of #33 (dependency check ported) |
| 45 | `4d86f4a10d` | Merge | Merge of #40 (Release PE cleaning enabled) |

## Deliberately excluded upstream frame-wait change

One hunk in `ea39ab8841` calls
`WaitForSingleObjectEx(nullptr, timeout, TRUE)` to pause a hot spin loop.
The Win32 API requires a valid waitable object handle; passing `nullptr`
fails rather than producing a sub-millisecond sleep. The fork's existing
wait implementation is retained. Any future low-CPU wait should use a
valid timer or an explicitly justified scheduling primitive, with care
not to delay frame completion.

## Camera and output-path tradeoffs

The upstream `RightStickLookProcess_832BB018` experimental native-injection
mode remains off by default upstream and wasn't imported into the fork's
different mouse/camera pipeline. Its behavior and the earlier
`LookStickProcess_83045778` refactors require a separate in-game evaluation.
Likewise, upstream's build `out/build/win-amd64-release` packaging path was
not copied because the fork installs the launcher and game in `out/game`.
The adapted packager explicitly excludes disc dumps and user files.

## SDK source strategy

The SDK submodule now points at the writable `MOTHblank/rexglue-sdk` fork
and incorporates the newer `himdo/rexglue-sdk` source along with five
Fable-specific input and renderer patch sets baked into the SDK revision.
The source preparation script verifies these integrations rather than
reapplying them on a matching revision.

## Outstanding verification

A Windows build and gameplay session are still required to establish that
the ported native memory/inflate implementations, hand-tuned overrides,
controller remapping, SDK-dependent renderer paths, packaging and Release
PE cleaning function together. Source-level review and GitHub commit
presence should not be mistaken for completed runtime validation.
