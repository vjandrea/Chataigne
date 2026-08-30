# Chataigne open-issue triage

Snapshot: 2026-08-30. 78 open issues on `benkuper/Chataigne`, 3 open PRs. Not a single open issue carries a label, so the repo has no triage signal at all right now.

Repo notes: this checkout is `vjandrea/Chataigne` (fork), `upstream = benkuper/Chataigne`. `gh` defaults to upstream for issues/PRs. Contribution path is fork plus PR, so "low-hanging fruit" below means "a PR you could realistically land", not "you can close it".

Legend for recommended action:
- **PR**: small, well-scoped, worth writing a fix
- **VERIFY**: probably already fixed on master, needs a build check then a close/comment
- **REPRO**: real bug, blocked on a minimal reproduction file
- **INVESTIGATE**: real bug, root cause partly known, needs real work
- **TRIAGE**: needs a maintainer decision (close, or label and park)

Every workable issue below carries a **Tests:** line: what test would encode the fix so it can't silently regress. See the "Testing baseline" section for the harness those lines assume.

---

## Contribution norms (verified 2026-08-30)

No `CONTRIBUTING.md`, no PR template, no code of conduct, on the fork or upstream (`gh api .../community/profile` returns nulls for all of them). The only written guidance:

- **README "Building the software"**: build needs the modified JUCE (`benkuper/JUCE` branch `develop-local`), Projucer-built, then the platform solution under `Builds/`. macOS expects JUCE at `~/JUCE` (symlink the bundled copy). Submodules must be cloned recursive (`juce_organicui`, `juce_timeline`, etc. are empty in this checkout right now).
- **Issue templates**: two only, "Bug report - Core code only !" and "Feature request". The bug one shouts that general questions and non-core bugs go to Discord. This is the basis for triage section 8.
- **Branch**: PRs target `master`. `develop_local` is the working branch, `juce8` is abandoned (per Ben in #153). Recent merged/open PRs (#332, #335, #336, #338) all target `master`.
- **Commit style**: informal, lowercase, no Conventional Commits ("try and fix shutdown again", "fix ci"). Match that in PRs since this is someone else's repo.
- **Review**: Ben reviews personally and is receptive ("PR are always welcome and I'll check it myself", #221). Emerick Herve (`EmerickH`) also triages and merges.
- **License**: GPL-3.0. New files need a GPL header consistent with the existing `/* ... */` block style.

Practical implication for the test work: there is no test job to hook into and no documented expectation of tests, so the first PR has to bring the harness with it (see "Proposed issue" at the bottom). Keep it optional and off the critical build path so Ben can ignore it if he wants.

---

## Testing baseline

**What exists:** no unit or integration tests. CI (`.github/workflows/build.yml`) has real but shallow smoke coverage: Linux AppImage started under `xvfb` with `--help` across ubuntu 22.04/24.04 + debian bookworm/trixie, a Raspberry payload `NEEDED`-symbol audit, a glibc <= 2.35 / OpenSSL ABI baseline check. No `.noisette` is ever loaded, no headless run, no clean-quit check, and no macOS or Windows runtime step (which is why #333 and #334 shipped in 1.10.4b1).

**What the Tests: lines assume we add** (details in the proposed issue):

1. **`--run-tests` flag** running `juce::UnitTestRunner` (JUCE built-in, no new dependency), wired into `ChataigneApplication::initialiseInternal` next to the existing `-headless` / `-forceNoGL` branching, exit nonzero on failure. For pure logic.
2. **Headless `.noisette` regression fixtures**: `test/fixtures/*.noisette` committed, run as `timeout 20 Chataigne fixture.noisette -headless`, assert exit 0. For "it crashes / hangs / does nothing" bugs. Also extend the existing smoke matrix to macOS + Windows.
3. **A `test/` folder** with the fixtures and a short README, plus a `tests` CI job (Linux, non-blocking at first).

Some logic (OSCQuery serialization, DMX/sACN packet building, curve LUT) lives in the `juce_organicui` / `juce_dmx` / `juce_timeline` submodules, not in this repo. Tests for those either go in the submodule repo or in a Chataigne-side test that exercises them through the public class. Flagged per issue.

---

## 1. PRs already open, just need review

| Issue | PR | State |
|---|---|---|
| #337 "Learn" not linking From Input Value to MIDI | #338 (5 add / 13 del, `ModuleManager.cpp`) | Ready. Tiny diff, demo video. Test locally and add a confirming comment. |
| #214 GPIO outdated for RPi5 | #335 `GPIOModule: replace vendored pigpio with libgpiod` | Needs a Pi 5 tester. Unblocks a fix sitting since Jan 2024. |
| (Ajazz AKP03) | #336 | Related to your own Ajazz module work, see [[ajazz_troubleshooting]]. |

**#337 Tests:** unit test the helper PR #338 introduces in `ModuleManager` (resolve the owning module of a nested value `Controllable`). Cases: value directly under a module, value in a nested container, value under a non-module container (expect null, no crash). This is pure lookup logic and is exactly what broke. A full Learn-flow test needs live input injection and is not worth it.

**#214 Tests:** hardware, no GPIO on CI runners. Add a headless fixture with a GPIO module and assert the app loads it and stays alive with no device present (graceful "no chip found" warning, not a crash). Belongs in the smoke matrix, not unit tests.

---

## 2. Low-hanging fruit (small PRs)

### #290 FLAC missing from audio file picker
FLAC decodes fine (JUCE `registerBasicFormats`), only the browse filter is restrictive. Concrete instance: `Source/Module/modules/audio/commands/PlayAudioFileCommand.cpp:24` sets `fileTypeFilter = "*.wav;*.mp3;*.aiff"` (no `flac`, no `ogg`). The sequence audio-layer picker has its own filter to find and widen too. One-liner per site. **PR.**
**Tests:** unit test asserting the filter wildcard is consistent with what `AudioFormatManager::registerBasicFormats()` actually produces, iterate the registered formats, assert every extension appears in the filter string. Invariant test, catches future drift when JUCE adds a format. Add a tiny committed `.flac` fixture and assert `formatManager.createReaderFor` returns non-null (guards against a build without the FLAC format compiled in).

### #273 "Launch Command File" on macOS with spaces in path
Current code at `OSExecCommand.cpp:96-116` already wraps `dir` and filename in escaped quotes for mac and linux (the Windows branch too). Likely fixed since the 1.9.24 report. **VERIFY**, then close or ask reporter to retest.
**Tests:** extract the per-platform command-string construction out of `OSExecCommand::triggerInternal` into a free function, `buildLaunchFileCommand(File dir, String filename, bool silent)` per `#if`. Unit test it with: path containing spaces, path containing a single quote, path containing `&&`, filename with `.sh` vs not. Textbook extract-pure-function-and-test. Doing this refactor is the fix even if the current code looks right, it makes the behaviour checkable.

### #202 OSC Command Tester keeps stale parameters when switching command
Ben (2023-10): "I disabled this feature for the next version." **VERIFY** on master, close if done.
**Tests:** mostly UI state (`ModuleCommandTesterEditor`), not cleanly unit-testable. If the underlying `ModuleCommandTester` holds command params in a model separate from the view, unit test that selecting command B after command A exposes B's param set exactly. Otherwise close on verify, no test.

### #197 OSCQuery: spurious `/foo/sync` in LISTEN commands
Ben confirmed "yes it's a bug". Isolated to the OSCQuery LISTEN path (`GenericOSCQueryModule`). **PR.**
**Tests:** unit test the LISTEN-command generation, given a synced container with parameters `a` and `b`, assert the emitted set of LISTEN paths is exactly `{/foo/a, /foo/b}` with no synthetic `/foo/sync`. Lives against `GenericOSCQueryModule`; the send can be intercepted by pointing the module at a loopback `OSCReceiver` in the test.

### #200 OSCQuery color serialized as `AARRGGBB` instead of spec `#RRGGBBAA`
Reporter cites the OSCQuery spec (`#RRGGBBAA`, leading hash). Fix is in color read/write, likely in `juce_organicui` OSCQuery helpers. Watch Chataigne-to-Chataigne compat (both ends wrong today). **PR** with a tolerant reader.
**Tests:** pure serialization, best unit-test target in the list. Table: opaque red -> `#FF0000FF`, transparent blue -> `#0000FF00`, opaque white -> `#FFFFFFFF`. Assert write matches spec and read accepts both the spec form and the legacy `AARRGGBB` form (round-trip). If the code is in `juce_organicui`, the test goes in that submodule's repo; note it in the PR.

### #185 Container `collapse()` from script updates the arrow but not the body
Clean repro (DS100 "Get Scenes"). UI/state desync between the disclosure triangle and the container collapsed flag. Small OrganicUI fix (`ControllableContainer` / its editor). **PR** (likely `juce_organicui`).
**Tests:** unit test at the container level, call the script-facing collapse setter, assert the persisted `editorIsCollapsed` (or equivalent) flag and the value the triangle binds to agree. Pure state. Submodule-side test.

### #201 Curve 2D Y axis points down, opposite of Point2D and dashboard
Ben: "yes that would be better". Sign flip on the Y projection in the Curve 2D editor, check saved-file coordinate compat. **PR**, small.
**Tests:** extract the value <-> normalized-position mapping from the Curve 2D editor component into a plain function and unit test it: a point at value `(0,1)` maps above a point at value `(0,0)` on screen, matching the Point2D convention. Add a load/save round-trip assertion on a fixture curve so old files don't flip.

### #224 sACN CID is all zeros, not unique
Two instances on one sACN net fight because gateways can't distinguish them. Fix: generate a per-module UUID once, persist in module params. **PR**, small-medium. Packet code is in `juce_dmx`.
**Tests:** unit test the sACN root-layer packet builder, assert the CID field is 16 bytes, non-zero, stable across repeated sends from one module instance, and distinct between two instances. Submodule-side (`juce_dmx`). Cheap and high-value: encodes exactly the reported failure.

### #288 Web dashboard: read-only text color ignored
Different repo: `benkuper/chataigne-web-dashboard`, `app/styles/app.scss:752`, `input:disabled { color: #18b5ef !important; }`. Drop or conditionalize `!important`. Confirmed by a second user. **PR** (other repo).
**Tests:** that repo is JS/SCSS. A Playwright/vitest DOM test: render a read-only string item with a custom text color, assert the computed color is the custom one, not `#18b5ef`. Out of scope for this repo's harness, note in the PR.

### #270 Compile fails on Fedora 41 (wiiuse `os_nix.c` implicit declaration)
Emerick (2025-03): "should have been fixed with the referenced PR". **VERIFY** on master, close.
**Tests:** compile-only regression. Add a Fedora container to the Linux CI build matrix (or at least a `-Werror=implicit-function-declaration` on the wiiuse C sources) so a missing prototype fails the build again. No runtime test.

---

## 3. Real bugs, need a reproduction file

### #292 Audio playback regression since 1.9.17 (choppy in nested sequences)
Confirmed by 3 users, Windows and macOS. `unclewalter`: start-time offsets near power-of-two ms boundaries clear the glitch, smells like buffer alignment in the nested-sequence audio path. Reporter promised a minimal `.noisette` June 2025, never delivered. **REPRO**: ping for it, or build one from `unclewalter`'s notes (master sequence with 2+ sub-sequences containing audio, second offset ~20 ms).
**Tests:** once repro'd, a headless fixture `test/fixtures/nested-audio.noisette` plus a debug counter on the audio callback (dropouts / buffer underruns). Run headless for N seconds, assert the counter stays 0. Needs a small test hook in the audio layer to expose the counter. This is the highest-value regression fixture in the list because it silently shipped and stuck for 8 versions.

### #321 / #291 / #154 Crashes with only a partial stack
- #321: crash after sustained OSC send to Resolume, Windows, stack truncated, no repro.
- #291: reproducible crash routing DMX input to OSC, has a `.noisette`.
- #154: crash on rapid `sendPOST` from a custom HTTP module, has a repro module.
#291 and #154 ship repro material. **INVESTIGATE** #291 first.
**Tests:** #291 -> commit the attached `.noisette` as a headless fixture (DMX-in ArtNet routed to OSC-out), drive it with a looped ArtNet sender from the test, assert survives 30 s. #154 -> headless fixture with the repro module hammering `sendPOST` in a tight loop, assert survives. Both are smoke/integration fixtures, not unit tests. These double as the first real entries in the crash-regression suite.

### #123 Bezier curve OSC value spikes at a fixed timecode
Ben diagnosed it: uniform-LUT creation, X-axis projection approximation error at a specific handle configuration. 2022, has a repro file, known area. Curve/LUT code is in `juce_timeline`. **INVESTIGATE.**
**Tests:** gold unit-test target. Rebuild the reported curve from its keyframes in code, sample the uniform LUT densely across the domain, assert no discontinuity larger than epsilon between adjacent samples (`abs(v[i+1]-v[i]) < k * expectedSlope`). The test literally is the bug. Submodule-side (`juce_timeline`), or Chataigne-side driving a `Sequence` mapping layer.

---

## 4. Bigger bugs with a known root cause

### #276 HTTPS requests crash with modern OpenSSL (self-compiled Linux)
Emerick root-caused it: `benkuper/juce_simpleweb` bundles ancient `libssl`/`libcrypto` `.so` under `libs/Linux/x86_64`, clashes with system OpenSSL 3.x. Deleting them (link system lib) fixes it; AppImage compat is the open question. **INVESTIGATE**, coordinate with #248.
**Tests:** the CI already has an ABI baseline audit, extend it: after the Linux build, `ldd` / `readelf -d` the binary and assert it does NOT resolve `libssl`/`libcrypto` to anything under the `juce_simpleweb` bundled path (assert system linkage). Plus a headless fixture doing one real HTTPS GET (the community modules URL is already fetched at startup) and asserting no crash, which the existing 20 s smoke run nearly covers already, make it explicit.

### #248 (+#289, #313, #270, #300) AppImage / packaging dependency mess on modern distros
`libcurl-gnutls.so.4`, `libssl1.1`, `CURL_GNUTLS_3`, `OPENSSL_3.2.0 not found`. Hits OpenSUSE Tumbleweed, Fedora 40/41, Ubuntu 24.04 on Pi, NixOS. One meta-problem: AppImage built against EOL libs. **TRIAGE**: consolidate into one tracking issue, needs an owner for the AppImage recipe. Highest user impact (every "won't start on Linux" traces here).
**Tests:** extend the existing AppImage smoke matrix. It already covers 4 Debian/Ubuntu images; add `fedora:41`, `opensuse/tumbleweed`, and (best effort) a NixOS container. Same check: extract, `ldd` for "not found", 20 s `xvfb` startup. This turns the whole bug cluster into a red/green CI signal and is the concrete deliverable for the tracking issue.

### #301 Crash: setting enum value (async EnumParameter notification after destruction)
Active thread, multiple users, all pointing at async notification / UI update arriving after the listener is destroyed, typically script-built dynamic enums. `dsokoloski`'s `MessageManagerLock` patch masks the assertions; Ben says wrong layer. **INVESTIGATE**, hard, but well-discussed. v1-line candidate.
**Tests:** the right tool is a TSan/ASan CI build, not a targeted unit test, run the existing smoke fixtures under `-fsanitize=thread`. Add a stress fixture: a script that rebuilds an `EnumParameter`'s option list on a timer while a mapping consumes it, headless, 30 s, must exit clean under ASan. A plain unit test can't reliably surface a race.

### #171 Stream Deck XL broke again on macOS Apple Silicon after 1.9.18
Worked <= 1.9.17, broke 1.9.18+ on M1, fine on Windows (`MrThyrox`, 2024-07). 18 comments, needs the hardware to bisect. **INVESTIGATE.**
**Tests:** HID device enumeration, hardware-bound. No CI test. If the model-ID parsing (report descriptor -> model enum) can be separated from the HID transport, unit test that with captured descriptor bytes for XL rev1/rev2. That is the part that regressed per the thread.

### #169 UI does not render with OpenGL renderer on some machines
Recurring 2023 to 2025-11. `-forceNoGL` is the workaround. Mitigation Ben floated: default OpenGL off. **TRIAGE**: decide the default.
**Tests:** if the decision is "default off", unit test the settings default value and the migration (old settings without the key -> off). GPU-driver rendering failure itself is not testable in CI.

---

## 5. NIC / networking cluster (one theme, several issues)

- #284 multiple NICs break sACN multicast (VirtualBox adapters)
- #220 OSC subnet broadcast not received
Both are "Chataigne binds the wrong interface". `calebwest-SS`: UDP/TCP modules already have a NIC selector; DMX, sACN, PSN do not. **PR**: port the interface-picker param to the multicast modules. Medium, high recurring-support value.
**Tests:** unit test the interface-selection helper, given a fake list of interfaces and a chosen name/IP, assert it returns the correct bind address, and a sane fallback when the chosen one is gone. The multicast join itself needs a loopback integration test (bind two sockets to a multicast group on the loopback, assert delivery) which is doable but flaky on CI runners, keep it non-blocking.

---

## 6. Stale, effectively deferred to Chataigne 2 / the Rust rewrite

Ben has said on the record these wait for v2. Not moving on v1, they clutter the list. Label `v2` (new label) or close with a pointer to a pinned roadmap issue. No tests, no work.

### Rust rewrite status (checked 2026-08-30)

Public, but not on the C++ repo. The real rewrite is **`Golden-Geek/Chataigne2`** (Ben's org). `benkuper/Chataigne2` is an empty placeholder ("Chataigne comes back."). No v2 branch exists on this C++ repo; its `ManagerRefactor` / `MultiEditingWIP` / `juce8` branches are unrelated C++ work.

- **Stack**: Rust + Svelte, self-contained Cargo/npm monorepo, no submodules. Tauri desktop shell (`Chataigne2`) over a reusable core (`Golden-Geek/golden_core`: `golden_model`, `golden_values`, `golden_parameters`, `golden_context`, `golden_graph`) and a reusable SvelteKit workbench (`Golden-Geek/golden_ui`). Same layering that OrganicUI gave v1. Rust owns the UI protocol and generates the TS transport bindings. The v1 mapping/filter/processor chain is replaced by a generalized node system ("Alchemist", also `Golden-Geek/golden_alchemist_*`); the state machine lives under `apps/chataigne/systems/state_machine`.
- **Scale / pace**: ~1200 commits Feb to Jul 2026. Strict Conventional Commits, branch names like `architecture/aaa-product-rewrite`, `agent/finish-sound-card-plan`. Agent-generated, matching Ben's #328 comment ("literally being coded by AI").
- **Current activity**: public v2 work paused around 27 to 30 Jul 2026 (`golden_core` / `golden_ui` last pushed 9 to 12 Jul). Since then Ben is back on the C++ line (personal `Chataigne` pushed 14 Aug, betas 1.10.4b1/b2, which is where #333 and #334 regressions originate). He is context-switching, and v1 is still the shipping product taking regressions.
- **Implications for this triage**:
  - The v1 test baseline (proposed issue below) is still worth doing: v1 keeps shipping betas and breaking, and v2 has no usable release. Frame it as backporting the rigor v2 already assumes (`Chataigne2`'s `.gitignore` lists `cargo-mutants`, so Ben is already planning mutation testing there).
  - Section 6 / 7 items marked "deferred to v2" are effectively "no ETA", not "coming soon". Treat a `v2` label as a polite close.
  - Do not invest in v1 rewrites of subsystems v2 already redid (node system, scripting engine, manager/grouping). Do invest in contained v1 bug fixes and packaging, which v2 does not retroactively fix for current users.

| Issue | Topic |
|---|---|
| #247 | sequence grouping ("major rewrite", pending 2+ years) |
| #317 | multi-select and mass editing (dead develop branch) |
| #316 | recording-mode improvements |
| #328 | MCP / LLM editing support (Ben: v2 already prepped) |
| #260 | JS boolean expression quirks (v2 moves to QuickJS) |
| #129 / #135 / #116 | template system bugs (whole system needs a revamp) |
| #96 | 2D mapping UI ergonomics |
| #87 / #242 | color type improvements, RGB to HSV (Ben resistant) |
| #150 | iOS / iPad support |

## 7. Feature requests, no traction, need a yes/no

| Issue | Note | Tests if done |
|---|---|---|
| #47 | JACK/LV2 CV, open since 2019, Ben lukewarm, no volunteer. Close or `help wanted`. | n/a |
| #133 | beat-link-trigger, needs C++ port of a Java lib. `help wanted` or close. | n/a |
| #153 | MQTT on Linux: Ben "Done and cooking" on master 2026-02. **VERIFY** and close. | headless fixture: MQTT module connects to a local broker, publish/subscribe round-trip, in the smoke suite |
| #155 | Stream Deck+, blocked on hardware. `help wanted`. | n/a |
| #183 | Homebrew cask, Ben happy to accept a PR. `help wanted`. | CI lint of the cask formula |
| #300 / #313 | Nix derivation, same root as #248. Fold into the packaging tracking issue. | covered by #248 smoke-matrix extension |
| #143 / #263 | Linux global keyboard read / send-keys, needs uinput or X work. `help wanted`. | unit test the keysym mapping table; runtime needs a real X session |
| #329 | Serial 8E1 etc. Reporter did the design work, named `SerialDevice::setParity/setStopBits/setDataBits`. Good **PR**, medium. | unit test: given module params `Parity=Even, StopBits=2`, assert the `SerialDevice` open call receives those; today it silently forces 8N1 |
| #221 | mouse scroll wheel: `calebwest-SS` has a working branch, blocked on Mac testing since 2024. Finish and PR. | unit test the scroll-event -> delta parsing per platform from captured event structs |

## 8. Not code issues, should be closed (Discord / user support)

Ben repeatedly redirects these to Discord. They will never be actioned and inflate the count. No tests.

#99, #136, #146, #173, #175, #176, #179, #204, #245, #256, #258, #283, #307 (empty body), #308, #310, #331, #241.

Close with a one-line "please continue on Discord, GitHub issues are for core code". Keep #258 (target editor chips vs `a>b>c`) and #331 (double OSC ping) open, both describe real UI/protocol bugs.
- **#331 Tests:** unit test the OSC send path, one `ping` command -> exactly one datagram on the wire (loopback `OSCReceiver`, assert count == 1). Encodes the double-send bug directly.
- **#258 Tests:** target-editor UI state, not cleanly unit-testable; leave untested.

---

## Suggested first moves

1. Test and comment on PR #338 (#337), free win sitting there.
2. Land the test harness (see proposed issue below) as its own small PR first, so later fix PRs have somewhere to put a test.
3. One PR: FLAC filter fix #290 + its invariant test. Bundle the VERIFY-and-close pass on #202 #270 #153 #273.
4. File the packaging tracking issue, link #248 #289 #313 #270 #300, and make its deliverable the smoke-matrix extension to Fedora/openSUSE/NixOS.
5. Propose a `v2` label to Ben and sweep section 6 into it.
6. With a Pi 5: #335 (#214) and #310 both wait on hardware.

---

## Proposed issue: testing baseline (harness + CI smoke coverage)

Not tied to any existing issue. Open this on `benkuper/Chataigne`, then link the per-issue test work above to it as a checklist / sub-tasks. Paste-ready below.

---

**Title:** Add a testing baseline: `--run-tests` unit harness + headless `.noisette` smoke fixtures

**Type:** enhancement (core / infrastructure)

**Body:**

Chataigne currently has no automated tests for application logic. CI builds on 4 platforms and runs a shallow Linux-only startup smoke check (`xvfb-run Chataigne --help` across 4 Debian/Ubuntu images, plus a Raspberry `NEEDED`-symbol audit and a glibc/OpenSSL ABI baseline). No `.noisette` is ever loaded, there is no headless-run check, no clean-quit check, and no runtime check on macOS or Windows. Two release-blocking regressions in 1.10.4b1 (#333 won't quit on macOS, #334 encoding on macOS) would have been caught by even a minimal headless load-and-quit test.

This issue proposes a low-footprint baseline that fix PRs can attach tests to. It is deliberately opt-in and off the critical build path.

**1. Unit harness (JUCE built-in, no new dependency)**

- Add a `--run-tests` command-line flag handled in `ChataigneApplication::initialiseInternal`, next to the existing `-headless` / `-forceNoGL` handling.
- When set: run `juce::UnitTestRunner`, print results, quit with a non-zero exit code on any failure, do not start the GUI.
- Add a `Source/Tests/` folder with `juce::UnitTest` subclasses (self-registering). First candidates, each tied to an open issue:
  - OSCQuery color serialization round-trip, spec `#RRGGBBAA` vs legacy `AARRGGBB` (#200)
  - OSCQuery LISTEN path generation, no synthetic `/sync` (#197)
  - audio file-type filter matches `AudioFormatManager` registered formats (#290)
  - launch-command string builder with spaces / quotes / `&&` in the path (#273)
  - serial module params reach `SerialDevice` (parity / stop bits / data bits) (#329)
- Logic that lives in submodules (`juce_organicui`, `juce_dmx`, `juce_timeline`: OSCQuery helpers, sACN packet build #224, curve uniform-LUT #123) gets its tests in the submodule repo, exercised here only through the public class.

**2. Headless `.noisette` smoke fixtures**

- Add `test/fixtures/*.noisette` (tiny, committed) and run each as `timeout 20 Chataigne fixture.noisette -headless`, asserting exit 0.
- First fixtures:
  - `smoke-minimal.noisette`: one mapping, sine generator to a dummy value. Catches engine-load and clean-quit regressions (#333, #310).
  - `smoke-dmx-route.noisette`: the `.noisette` attached to #291 (DMX-in routed to OSC-out), driven by a looped sender, assert no crash over 30 s.
  - `smoke-http-post.noisette`: the repro module from #154 hammering `sendPOST`, assert no crash.
- Extend the existing AppImage smoke matrix to `fedora:41`, `opensuse/tumbleweed` and, best effort, NixOS (#248, #289, #313, #300).
- Add the same `--help` (and, where a display is available, `-headless` fixture) startup check to the macOS and Windows CI jobs.

**3. CI**

- New `tests` job (Linux, Release build) running `Chataigne --run-tests` then the headless fixtures.
- Non-blocking (`continue-on-error`) initially so it does not gate releases while the suite is young; flip to blocking once stable.
- Optional follow-up: an ASan/TSan build of the smoke job for the async/threading crash class (#301).

**Non-goals:** testing hardware modules, UI components, or GPU rendering. Those stay on manual QA and the startup smoke check.
