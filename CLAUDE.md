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
- LoopyPuzzleBobble is also the source for sound. Integrate its music and
  sound-effect code directly; don't vendor a library.
- `G:\LoopySoundlib\loopy-soundlib`: unfinished. Use only its
  `docs/hardware.md` and `docs/instruments.md`, as reference.
- The user's own `G:\Loopy*` projects are GPL-2.0-or-later, and code moves
  freely between them. Two are **not** the user's, so credit them:
  - `G:\LoopyDoom` (ThroatyMumbo, GPL-2.0-or-later). LoopyManiac's print code
    comes from it.
  - LoopyMSE (kasami, GPL-3; only forked). Use facts from it, never code.
- Kasami's loopy-homebrew-template is zlib; keep its notice wherever the boot
  or build skeleton is used.
- `G:\LoopySimCity`: ignore, don't use anything from it.
