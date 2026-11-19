# Decisions

Decisions taken autonomously during the unattended Phase 1 run. Each entry: what, why.

## Toolchain
- **Compiler kit: MSVC (Visual Studio 18 Community, v145 toolset) with Qt `msvc2022_64`.**
  vswhere found the C++ workload, so the preferred kit is used. The v145 toolset is
  ABI-compatible with the msvc2022 Qt binaries (MSVC 2015+ binary compatibility).
- **Build environment:** `vcvars64.bat` is called inside a throw-away `cmd` per build; the
  global PATH is never modified. CMake and Ninja are referenced by full path via
  `CMakeUserPresets.json` (git-ignored).
- **Running from the build folder:** tests get the Qt `bin` folder prepended to their own
  PATH through the CTest `ENVIRONMENT_MODIFICATION` property (derived from the Qt target,
  nothing hard-coded). The app is made runnable with `windeployqt` into the build folder.
