#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Rebake every song in assets/music (see SOURCES.md there) into
# src/music_<name>.h, with the polyphony and wire checks. A song whose hand
# exceeds a console channel's voices is split first. The outputs are
# committed, so the cart build does not run this.
#
# Not --strict: an octave-fold warning (a note outside MIDI 36-96) is exactly
# what the synth would do anyway. Read the polyphony and wire report instead.
#
# Usage: python tools/sound/mkmusic.py

import os
import subprocess
import sys

CART = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(CART, "tools", "sound")

# name -> (source MIDI, TOML mapping, optional voice split: (channel, overflow, voices))
SONGS = {
    "candeur": ("assets/music/burgmuller_la_candeur.mid", "assets/music/candeur.toml", None),
    "pastorale": ("assets/music/burgmuller_la_pastorale.mid", "assets/music/pastorale.toml", None),
    "clementi": ("assets/music/clementi_op36no1_allegro.mid", "assets/music/clementi.toml", None),
    "gymnopedie": ("assets/music/gymnopedie_1.mid", "assets/music/gymnopedie.toml", (1, 2, 4)),
}


def run(*args):
    print("$", " ".join(args))
    subprocess.run([sys.executable, *args], check=True, cwd=CART)


def main():
    os.makedirs(os.path.join(CART, "build"), exist_ok=True)
    for name, (midi, toml, split) in SONGS.items():
        if split:
            channel, overflow, voices = split
            out = f"build/{name}.mid"
            run(os.path.join(TOOLS, "split_voices.py"), midi, out,
                "--channel", str(channel), "--overflow", str(overflow),
                "--voices", str(voices))
            midi = out
        run(os.path.join(TOOLS, "lps_bake.py"), midi, toml, "--check-poly",
            "--simulate", "--name", name, "-o", f"src/music_{name}.h")


if __name__ == "__main__":
    main()
