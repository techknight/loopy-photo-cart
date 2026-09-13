# Hardware test session

## Results, 2026-09-13

- **Printing:** landscape stickers look good. Portrait photos print rotated
  to the right, as designed. No cassette gives "NO CASSETTE" and no print. A
  temporary sticker jam was reported and recovered. Controls and music carry
  on perfectly after a print. Cancelling mid-print isn't supported by the
  console.
- **Changes from feedback:**
  - the print result now disappears by itself, with no "A: OK" to invite a
    mis-press;
  - L/R in the viewer make the d-pad sound;
  - the music changes to something upbeat and subtle.
- **Sticker shape:**
  - only 224 of the 241 lines print (bottom border missing);
  - dots measure 0.160 × 0.1425 mm, so the circle came out wide on the
    sticker but round on the TV.

  Photos are now 256×224 dots, cropped to the printed 1.283 : 1 shape and
  letterboxed on the TV (`docs/PLAN.md` §8).
- **Sound:** music and effects implemented flawlessly.
- **Screen:** everything is great.

## Next session: things to confirm

| Check | ROM |
|---|---|
| Stickers now show the whole photo, and a circle prints round (`calibration.bin` chart 1) | `cart\build\calibration.bin` |
| Photos in the viewer have black bars above and below (landscape) or at the sides (portrait) and look right | `cart\build\fixture-demo.bin` |
| The print result goes away by itself | any |
| Pick the music: *La Candeur* (default), *La Pastorale*, Clementi Op. 36 No. 1, or Gymnopédie | `cart\build\music-candeur.bin`, `music-pastorale.bin`, `music-clementi.bin`, `music-gymnopedie.bin` |

Build the music ROMs with:

```
cart\scripts\build.ps1 EXTRA_CFLAGS=-DLPC_MUSIC_PASTORALE
python cart\tools\mkfixture.py --out cart\build\music-pastorale.bin demo\photos\*.jpg
```

Use `_CLEMENTI` or `_GYMNOPEDIE` for the other songs, then run
`cart\scripts\build.ps1` again for the normal template.

The checklist below is kept for future sessions.

Everything below has been checked in LoopyMSE. These are the things only a
real Loopy, Floopy Drive and sticker cassette can confirm. Stickers cost
money, so the printing tests are batched into one sitting.

## ROMs to load

Build the template and ROMs first (`cart\README.md`):

```
cart\scripts\build.ps1
python cart\tools\mkfixture.py --out cart\build\fixture-demo.bin demo\photos\*.jpg
python cart\tools\mkcalibration.py
```

| ROM | What it's for |
|---|---|
| `cart\build\fixture-demo.bin` | The 25 demo photos on 3 grid pages: everyday use, sound, printing |
| `cart\build\calibration.bin` | Two test charts for measuring the printed sticker |

## 1. Printing (about 5 stickers)

Use `fixture-demo.bin` unless noted.

| # | Do this | Expect | Note down |
|---|---|---|---|
| 1 | Grid: highlight a landscape photo, press **Start**, then **A** | "PRINT PHOTO?" → "PRINTING..." → a sticker → "PRINTED!" | Does the sticker look like the screen? Colours, brightness |
| 2 | Viewer: open a **portrait** photo (e.g. the flamingos), press **A** and **A** again | The sticker is the photo turned 90°, filling the sticker lengthwise | Which way is it turned? |
| 3 | Take the cassette out, press **Start** on the grid, then **A** | "NO CASSETTE", no printing | |
| 4 | Start a print and cancel it on the console if the Loopy allows | "CANCELLED" (or "PRINT FAILED") | What the console does |
| 5 | After every print: move the cursor, open photos, turn pages | Controls respond straight away; nothing is stuck; the cursor ring looks right | Any sluggishness, stuck buttons, sprite glitches |
| 6 | After a print, listen | The music carries on; no note hangs | Stuck or droning notes |

## 2. Sticker shape (2 stickers, `calibration.bin`)

Print both charts: photo 1 (landscape) and photo 2 (portrait). On each sticker,
measure in millimetres:

| Measure | Chart 1 | Chart 2 |
|---|---|---|
| Whole printed image: width × height (edge to edge of the red border) | | |
| Green horizontal bar length | | |
| Green vertical bar length | | |
| Is the blue circle round, or wider than tall / taller than wide? | | |
| Where does the red "UP" arrow point on the sticker? | | |

What the numbers are for:
- **Crop box shape:** width ÷ height of chart 1 becomes `STICKER_ASPECT`, the
  shape the web app's crop box uses (currently a 4:3 guess).
- **Dot shape:** the horizontal bar ÷ the vertical bar gives the shape of a
  printer dot. If it isn't 1, photos are resampled to compensate.
- **Portrait rotation:** chart 2's arrow says whether portrait photos should
  be rotated the other way.

Also note if any row or column at the edges is cut off: LoopyMSE shows only
224 of the 241 printed lines, so the bottom of the sticker is unverified.

## 3. Sound

| Check | Note down |
|---|---|
| Does the Gymnopédie piano (preset 1) sound pleasant, or harsh or wrong? | If wrong, other presets can be auditioned |
| Balance: is the music too loud or quiet next to the menu sounds? | |
| D-pad tick and button chirp: do they sound like menu sounds? Too sharp? | |
| **C** pauses and resumes the music | |
| Leave it running 10+ minutes: does the loop restart cleanly with no stuck notes? | |

## 4. Screen

| Check | Note down |
|---|---|
| Grid header ("PHOTOS", "A: View / D: Show Help", "1/3") fully visible on the TV? | Anything cut off at the edges |
| **D** help popover: all text visible and readable? | |
| Viewer counter ("3/25") appears top right and disappears | |
| Anything flickers, tears or shows garbage | |

## What to send back

The measurements from section 2, and short notes from sections 1, 3 and 4.
Photos of the stickers next to a ruler help too.
