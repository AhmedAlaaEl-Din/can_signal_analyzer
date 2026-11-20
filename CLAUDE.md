# CLAUDE.md — project rules for Claude Code

This is a personal project, completely independent from any employer.

## Hard safety rules
- No real CAN hardware, drivers, DBC files, captures, tool configs or specs from any
  work source. All frames are simulated with invented IDs and names.
- Do not read files outside this project folder and the toolchain folders
  (the Qt installation that `QT_ROOT` points to, and the Visual Studio install found via
  vswhere).
- No network access, no accounts, no telemetry, no downloads. Only the
  already-installed toolchain. Never install anything or modify PATH globally.
- Never hard-code machine paths in tracked files. Use `CMakePresets.json` with the
  `QT_ROOT` environment variable; machine values go in the git-ignored
  `CMakeUserPresets.json`.
- Never `git push` or merge without asking. Work on feature branches, small commits.
- Do not read or commit `.env` files or other secrets.

## Working style (unattended runs)
- Do not stop to ask; choose the safest reasonable option and log it in
  `docs/decisions.md`. Anything that needs the owner goes in `docs/needs-me.md`.
- Keep `docs/progress-log.md` (timestamp + what finished).
- If the same problem fails 3 times: log it and move on. Never loop.

## Layout
- `src/core/` — C++20 core, Qt Core/Gui only, no Qt Quick dependency.
- `src/app/` — QML UI (Theme singleton, custom controls, screens).
- `tests/` — Qt Test suites run via CTest.
- `docs/` — design system, decisions, progress log, explainers.

## Build
See README.md (CMake presets, `QT_ROOT`).
