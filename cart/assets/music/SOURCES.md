# Music sources

Every piece here is a public-domain composition, taken from a
[Mutopia Project](https://www.mutopiaproject.org) edition that its typesetter
placed in the public domain. Each piece-info page shows License: "Public
Domain", and the `.ly` files say so in their `license`, `copyright` or
`tagline` fields. The `.mid` and `.ly` files are unmodified downloads from
2026-09-13.

`python tools/sound/mkmusic.py` rebakes every song below into
`src/music_<name>.h`. The outputs are committed, so the cart build doesn't run
it.

| Song | Header symbol | Length | Notes | ROM | Notes at once (peak / limit) | Wire (500 bytes/s available) |
|---|---|---|---|---|---|---|
| La Candeur | `lps_song_candeur` | 36 s | 241 | 1.9 KB | ch0 3/6, ch1 3/4 | 29 bytes/s, high-water 19/63, no drops |
| La Pastorale | `lps_song_pastorale` | 51 s | 355 | 2.9 KB | ch0 1/6, ch1 4/4 | 31 bytes/s, high-water 24/63, no drops |
| Clementi Op. 36 No. 1, Allegro | `lps_song_clementi` | 58 s | 333 | 2.7 KB | ch0 2/6, ch1 1/4 | 27 bytes/s, high-water 13/63, no drops |
| Gymnopédie No. 1 | `lps_song_gymnopedie` | 141 s | 282 | 2.3 KB | ch0 5/6, ch1 4/4, ch3 1/4 | 9 bytes/s, high-water 30/63, no drops |

**The cart plays La Candeur, then Clementi, then repeats**
(`cart/platform/lp_sound.c`), with a short breath between songs. This
replaced Gymnopédie after the hardware test asked for something upbeat and
subtle. Those two are baked without looping (`loop = false` in their TOML) so
the playlist can move on. The others still loop and stay for auditioning:
build with `EXTRA_CFLAGS=-DLPC_MUSIC_PASTORALE`, `-DLPC_MUSIC_CLEMENTI` or
`-DLPC_MUSIC_GYMNOPEDIE` to hear one song on its own.

All four use program 1, the ROM's piano-like preset. Every piece
starts on its first note, with no silence. Console channel 2 is left free for
the sound effects.

## La Candeur — J. F. F. Burgmüller, Op. 100 No. 1

| Field | Value |
|---|---|
| Edition | Mutopia #202, typeset by Bas Wassink from Collection Litolff (19th century) |
| Page | <https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=202> |
| Files | `burgmuller_la_candeur.mid`, `burgmuller_la_candeur.ly` (Mutopia's `25EF-01.*`) |
| Licence | Public Domain. The `.ly` tagline reads: "This sheet music has been placed in the public domain by the typesetter". |
| Tempo | ♩ = 152 (Allegro moderato), as scored |

```
python tools/sound/lps_bake.py assets/music/burgmuller_la_candeur.mid assets/music/candeur.toml --check-poly --simulate --name candeur -o src/music_candeur.h
```

## La Pastorale — J. F. F. Burgmüller, Op. 100 No. 3

| Field | Value |
|---|---|
| Edition | Mutopia #218, typeset by Bas Wassink from Collection Litolff (19th century) |
| Page | <https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=218> |
| Files | `burgmuller_la_pastorale.mid`, `burgmuller_la_pastorale.ly` (Mutopia's `25EF-03.*`) |
| Licence | Public Domain. The `.ly` tagline reads: "This sheet music has been placed in the public domain by the typesetter". |
| Tempo | ♩ = 100, as scored |

```
python tools/sound/lps_bake.py assets/music/burgmuller_la_pastorale.mid assets/music/pastorale.toml --check-poly --simulate --name pastorale -o src/music_pastorale.h
```

## Sonatina Op. 36 No. 1, first movement — Muzio Clementi

| Field | Value |
|---|---|
| Edition | Mutopia #804, typeset by Brian D. Rude from the Sonatina Album (G. Schirmer, 1893) |
| Page | <https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=804> |
| Files | `clementi_op36no1.ly`; `clementi_op36no1-mids.zip`, the edition's MIDI download; `clementi_op36no1_allegro.mid`, that zip's `sonatina-1.mid` extracted unchanged. It is the first movement, ♩ = 156. |
| Licence | Public Domain. The `.ly` file has `license = "Public Domain"`, and its copyright line reads: "Placed in the public domain by the typesetter". |

The zip also holds the second movement (`sonatina-1-1.mid`, Andante, ♩ = 92)
and the third (`sonatina-1-2.mid`, Vivace, ♪ = 160). Neither is baked.

```
python tools/sound/lps_bake.py assets/music/clementi_op36no1_allegro.mid assets/music/clementi.toml --check-poly --simulate --name clementi -o src/music_clementi.h
```

## Gymnopédie No. 1 — Erik Satie (1888)

| Field | Value |
|---|---|
| Edition | Mutopia Project #37, typeset by Evin Robertson from the Dover edition |
| Page | <https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=37> |
| Files | `gymnopedie_1.mid`, `gymnopedie_1.ly` |
| Licence | Public Domain. The composition is long out of copyright, and the Mutopia edition is placed in the public domain by its typesetter (see the `license` and `copyright` fields in `gymnopedie_1.ly`). |

The left hand peaks at five notes, one more than a console channel allows, so
its extra voice is split onto its own channel before baking:

```
python tools/sound/split_voices.py assets/music/gymnopedie_1.mid build/gymnopedie.mid --channel 1 --overflow 2 --voices 4
python tools/sound/lps_bake.py build/gymnopedie.mid assets/music/gymnopedie.toml --check-poly --simulate --name gymnopedie -o src/music_gymnopedie.h
```

Three bass notes below MIDI 36 are raised an octave, as the synth would do
anyway.
