# Needs me

Items that need the owner's attention or decision. Nothing is blocking.

1. **Review and push.** All work is on local branch `phase1-foundation` (not pushed, not
   merged), on top of a one-commit `main`.
2. **Video.** No video tool is available; `evidence/frames.zip` holds 224 PNG frames at
   5 fps (≈45 s, 1600×1000 px because Windows display scaling is 125 %). Turn them into a
   video with any tool, e.g. `ffmpeg -framerate 5 -i frame_%04d.png -c:v libx264
   -pix_fmt yuv420p demo.mp4`, then put the link into the README placeholder.
3. **Toolchain allow rules** are in the git-ignored `.claude/settings.local.json`; on a
   new machine, create that file with your own Qt/CMake/Ninja paths if you want them
   pre-approved.
4. **Git identity.** Commits use your global git identity; the commit message trailer names
   the assistant as co-author. Amend if you want a different attribution.
5. **Qt Creator kits.** The Qt installer registered its kits with paths on a different drive
   than the actual Qt installation. Follow "Opening the project in Qt Creator 21" in the README to create a
   working kit once. Optional: install "Debugging Tools for Windows" (Windows SDK) if you
   want to debug with the MSVC kit; I did not install anything.

## Phase 2
6. **Review branch `phase2-decoding`** (not pushed, not merged).
7. **Video:** `evidence/frames.zip` holds 541 JPEG frames at exactly 12 fps (1280 × 800).
   Example: `ffmpeg -framerate 12 -i frame_%04d.jpg -c:v libx264 -pix_fmt yuv420p demo.mp4`.
8. **README screenshots:** done (Signals and Signal detail, dark, in `docs/img/`).
9. **Try the file dialog once by hand** (*Open DBC…* / Ctrl+O): it could not be clicked in an
   unattended run; the loading code behind it is tested.

## Phase 3
10. **Review branch `phase3-plots`** (not pushed, not merged).
11. **Video:** `evidence/frames.zip` holds 541 JPEG frames at exactly 12 fps (1280 × 800).
    `ffmpeg -framerate 12 -i frame_%04d.jpg -c:v libx264 -pix_fmt yuv420p demo.mp4`.
12. **Qt Graphs comparison (optional):** Qt Graphs is not installed here, so the benchmark
    compared the custom engine with Qt Charts. If you want the Graphs numbers, install the
    "Qt Graphs" module with the Qt Maintenance Tool; I did not install anything.
13. **Try by hand once:** dragging a signal from the Plots picker onto a lane, and the PNG/CSV
    save dialogs (both exports were verified programmatically, not through the dialogs).
14. ~~README screenshots~~ — done: `docs/img/plots-cursors-dark.png`, `docs/img/plots-tooltip-light.png`.


## Phase 4
15. **Review branch `phase4-release`** and the local tag **`v1.0.0`** (neither is pushed).
    Push the tag only together with the branch, if you want it (`git push origin v1.0.0`).
16. ~~**Video:** `frames.zip` in the hand-off folder (outside the repository; the path is in the
    chat summary) holds 721 JPEG frames at exactly 12 fps (1280 × 800),
    a 60 s scene. `ffmpeg -framerate 12 -i frame_%04d.jpg -c:v libx264 -pix_fmt yuv420p demo.mp4`.~~ — done:
    unlisted on YouTube, https://youtu.be/ILNY42n71yI, linked at the top of the README.
17. **License choice:** the repository now has an **MIT** `LICENSE` with the copyright line
    "The CAN Analyzer author" (no name, per the rules). Change the holder name or the license
    if you prefer before publishing.
18. **Qt LGPL source offer:** `THIRD_PARTY/Qt/NOTICE.md` points to the official Qt source
    download for the exact version. If you want to be strictly on the safe side for public
    binary releases, add your own written offer or host the Qt 6.12.0 source archive next to
    the release; I did not make that commitment on your behalf.
19. **Installer:** not built. No installer tool (NSIS, WiX, Inno Setup, Qt Installer
    Framework) is installed and installing one was not allowed. The portable zip is the
    release.
20. **Try by hand once:** the Record save dialog, Open log dialog, drag-and-drop of a log onto
    the window, and the About links (they open the license files with your default text
    editor). These were verified programmatically, not through the native dialogs.
21. **README screenshots (optional):** copy the Phase 4 screenshots you like from
    `evidence/screenshots/` (or the hand-off copy) to `docs/img/` and un-comment the placeholder block at the top of
    the README.
22. **Settings location:** the app now remembers its settings in
    `HKEY_CURRENT_USER\Software\CanAnalyzer`. Delete that key to reset to the first-run state.
