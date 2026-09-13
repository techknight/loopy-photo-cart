# Loopy Photo Cart

See `docs/PLAN.md` for the architecture and milestones.

## Rules

- LF (`\n`) line endings for every file. Tools must write bytes or explicit `\n`.
- Run git from PowerShell or Git Bash, never inside WSL.
- Build the cart in WSL with the Wonderful `sh-elf-gcc` toolchain. Don't put
  heredocs inside `wsl -e bash -c "..."`; write a script file instead.
- Photos must never leave the user's browser: no uploads, analytics or
  third-party requests in `web/`.

## Reference projects

- `G:\LoopyManiac\loopy-maniac`: sticker printing (`platform/lp_print.c`,
  `src/loopycam.c`) and recovery after a print (input, clock, sound).
- `G:\LoopyPuzzleBobble\loopy-puzzlebobble`: frame rate and video (`docs/perf.md`,
  `platform/lp_video.c`), input, and LoopyMSE capture scripts.
- `G:\LoopySoundlib\loopy-soundlib`: music and sound effects (`lps/`,
  `tools/lps_bake.py`, `docs/hardware.md`, `docs/instruments.md`). It's the
  user's own code (GPL-2.0-or-later). Don't take anything that pulls in the
  loopy-glider MIT licence.
- `G:\LoopySimCity`: ignore, don't use anything from it.
