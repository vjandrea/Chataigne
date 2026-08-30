# Chataigne : working notes

Show-control glue (noise / OSC / DMX / MIDI / serial ...) by Ben Kuper. C++ on a modified JUCE fork. GPL-3.0.

This file records what isn't obvious from the code. Anything about code structure, read the code.

## Repo topology

- `origin` = `git@github.com:vjandrea/Chataigne.git` (Andrea's fork).
- `upstream` = `git@github.com:benkuper/Chataigne.git` (Ben's, the real one).
- `gh` resolves to `benkuper/Chataigne` by default, so issues/PRs you list are upstream's. Pass `--repo` when you mean the fork.
- Contribution path: branch on the fork, PR against `benkuper/Chataigne` `master`. No CONTRIBUTING.md, no PR template, no code of conduct. Issue labels exist but are unused.
- Bug-report template tells general questions and non-core bugs to go to Discord, not the tracker.

## Build

- Projucer only. `Chataigne.jucer` is the source of truth (`projectType="guiapp"`, C++17). No CMake, don't add one.
- Needs `benkuper/JUCE` branch `develop-local`. Build the Projucer from that checkout first.
- Clone with `--recursive`. Submodules live under `Modules/`: `juce_organicui`, `juce_timeline`, `juce_simpleweb`, `juce_sharedtexture`, `juce_serial`, `juce_dmx`. If they're empty, `git submodule update --init`.
- macOS expects JUCE at `~/JUCE` (symlink the bundled copy per the README).
- `OrganicUI` (`juce_organicui`) is the shared framework base, same role that `golden_core` plays for v2. App-generic behavior (CLI parsing, window lifecycle) lives there, not in this repo.
- Entry point: `Source/Main.cpp`, `ChataigneApplication`. `-headless` and `-forceNoGL` are parsed in `OrganicApplication` (submodule), not here.

## Tests / CI

- No unit or integration tests. Zero.
- CI is `.github/workflows/build.yml`: build + package for Win/macOS/Linux/Raspberry, plus a Linux-only AppImage smoke test (`xvfb-run "$APP" --help` across a few Ubuntu/Debian images), a Raspberry NEEDED-symbol audit, and a glibc/OpenSSL ABI baseline check. No `.noisette` load, no headless run, no clean-quit check, nothing on macOS/Windows runtime.
- Adding tests: JUCE ships `juce::UnitTest` + `UnitTestRunner`, wire it behind a `--run-tests` CLI flag in `Main.cpp`. No new dependency. See `docs/ISSUES_TRIAGE.md` for the laddered plan and the paste-ready tracking issue.

## Chataigne 2 (Rust rewrite)

- Separate repo: `Golden-Geek/Chataigne2` (Rust + Svelte monorepo, Tauri shell, reusable `Golden-Geek/golden_core` + `golden_ui`). `benkuper/Chataigne2` is an empty placeholder.
- No v2 branch on this C++ repo. `ManagerRefactor` / `MultiEditingWIP` / `juce8` here are unrelated C++ work.
- Public v2 work paused ~late Jul 2026. Ben is back on the C++ line (betas `1.10.4b1` / `b2` on `master`), which is where the recent regressions come from. v1 is still the shipping product.
- Practical read: don't rewrite v1 subsystems v2 already redid; do fix contained v1 bugs and packaging.

## This branch

`docs/issue-triage` carries research/spec markdown that shouldn't clutter `master`. Currently `docs/ISSUES_TRIAGE.md` (full triage of open issues with per-issue test notes) and this file.
