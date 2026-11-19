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
14. **README screenshots (optional):** copy plot screenshots from `evidence/screenshots/` to
    `docs/img/` and un-comment the Phase 3 placeholders in the README.

