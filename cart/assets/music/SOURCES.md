# Music sources

## Gymnopédie No. 1 — Erik Satie (1888)

| Field | Value |
|---|---|
| Edition | Mutopia Project #37, typeset by Evin Robertson from the Dover edition |
| Page | <https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=37> |
| Files | `gymnopedie_1.mid`, `gymnopedie_1.ly` (unmodified downloads, 2026-09-13) |
| Licence | Public Domain. The composition is long out of copyright, and the Mutopia edition is placed in the public domain by its typesetter (see the `license` and `copyright` fields in `gymnopedie_1.ly`). |

The ROM's copy is built from it in two steps. Both outputs are committed, so
the cart build needs neither step:

```
python tools/sound/split_voices.py assets/music/gymnopedie_1.mid build/gymnopedie.mid --channel 1 --overflow 2 --voices 4
python tools/sound/lps_bake.py build/gymnopedie.mid assets/music/gymnopedie.toml --check-poly --simulate --name gymnopedie -o src/music_gymnopedie.h
```

The first step writes `build/gymnopedie.mid`. The bake reports:
- notes at once, peak against the limit: channel 0 5 of 6, channel 1 4 of 4,
  channel 3 1 of 4;
- 9 bytes/s on the wire against 500, with no dropped notes;
- three bass notes below MIDI 36 raised an octave, as the synth would do
  anyway.

`cart/tools/sound/mkmusic.py` runs both steps.
