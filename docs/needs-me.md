# Needs me

Items that need the owner's attention or decision. Nothing is blocking.

1. **Review and push.** All work is on local branch `phase1-foundation` (not pushed, not
   merged), on top of a one-commit `main`.
2. **Video.** No video tool is available; `evidence/frames.zip` holds 224 PNG frames at
   5 fps (≈45 s, 1600×1000 px because Windows display scaling is 125 %). Turn them into a
   video with any tool, e.g. `ffmpeg -framerate 5 -i frame_%04d.png -c:v libx264
   -pix_fmt yuv420p demo.mp4`, then put the link into the README placeholder.
3. **README screenshots.** The README has image placeholders under `docs/img/`. Copy the
   screenshots you like from `evidence/screenshots/` there (they are git-ignored by default
   under `evidence/`) if you want them in the repository.
4. **Toolchain allow rules** are in the git-ignored `.claude/settings.local.json`; on a
   new machine, create that file with your own Qt/CMake/Ninja paths if you want them
   pre-approved.
5. **Git identity.** Commits use your global git identity; the commit message trailer names
   the assistant as co-author. Amend if you want a different attribution.
