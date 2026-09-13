#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Rebuild src/music_gymnopedie.h from assets/music (see SOURCES.md there):
# split the left hand's fifth voice onto its own channel, then bake with the
# polyphony and wire checks. The output is committed, so the cart build does
# not run this.
#
# Usage: python tools/sound/mkmusic.py

import os
import subprocess
import sys

CART = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(CART, "tools", "sound")


def run(*args):
    print("$", " ".join(args))
    subprocess.run([sys.executable, *args], check=True, cwd=CART)


def main():
    os.makedirs(os.path.join(CART, "build"), exist_ok=True)
    run(os.path.join(TOOLS, "split_voices.py"), "assets/music/gymnopedie_1.mid",
        "build/gymnopedie.mid", "--channel", "1", "--overflow", "2", "--voices", "4")
    # Not --strict: three low bass notes (below MIDI 36) are octave-folded,
    # which is a warning but exactly what the synth would do anyway. The
    # polyphony and wire checks are read from the report.
    run(os.path.join(TOOLS, "lps_bake.py"), "build/gymnopedie.mid",
        "assets/music/gymnopedie.toml", "--check-poly", "--simulate",
        "--name", "gymnopedie", "-o", "src/music_gymnopedie.h")


if __name__ == "__main__":
    main()
