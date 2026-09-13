# Loopy Photo Cart — Development Plan

A static web app, hosted on GitHub Pages, that turns a user's photos into a
Casio Loopy ROM. The ROM shows the photos as pages of thumbnails and a
full-screen viewer, and prints any photo on the Loopy's sticker printer.
**All image processing and ROM assembly happen in the user's browser; no photo
ever leaves the machine.**

---

## 1. Is the "template ROM + appended data" idea feasible?

Yes. It is the core of the design:

1. The cart program is compiled **once** (in WSL/CI) into
   `loopy-photo-cart-template.bin`. It holds no photos.
2. The linker reserves a fixed code region. The **photo pack** always starts at
   a fixed address, `PACK_BASE = 0x0E040000` (file offset `0x40000`, which
   leaves 256 KB for code, font and UI art). A linker `ASSERT` fails the
   build if the code ever grows past it.
3. The web app:
   1. loads the template;
   2. truncates it to `0x40000` bytes;
   3. appends the pack it built;
   4. pads with `0xFF` to an even 4 KB boundary;
   5. patches the header's `romlast` (offset 4) and `checksum` (offset 8).

   This is the same algorithm as `tools/fixrom.py`: a 32-bit sum of the
   big-endian 16-bit words from `romfirst` to `romlast+2`.
4. At boot the cart validates the pack at `PACK_BASE` (magic, format version,
   CRC32). If the pack is missing or invalid, it shows a "No photos in this
   cartridge" screen instead of crashing. The bare template stays bootable,
   which also makes it a useful test.

The template carries a 32-byte **descriptor** at ROM offset `0x20`, in the
cartridge header's free space: magic `LPCT`, descriptor and pack-format
versions, `PACK_BASE`, the 4 MB limit, and the build id (`cart/README.md`). The web app reads
it and refuses a template/app version mismatch, so an old cached template can
never silently produce a broken ROM.

A JS/TS copy of the checksum must match `fixrom.py` byte-for-byte; a
golden-file test checks this (§7).

---

## 2. Hardware facts this design rests on

These come from LoopyManiac (printing) and LoopyPuzzleBobble (performance).
File references are to those repos.

| Topic | Fact | Consequence |
|---|---|---|
| Video mode | 256×240 (`VIDEO_HEIGHT_240P`, Maniac). Bitmap 8bpp; 256 RGB555 palette entries shared by all layers | One photo = one 256-colour image plus palette |
| Page flip | `BM_MODE_8BPP_SHARED` gives two pages. Draw in RAM, DMA-blit into the hidden page, flip with one `BM_SCROLLY[0]` write after `bios_vsync()` | Tear-free photo changes |
| Full-screen blit cost | About 8 ms on the console for a full frame by DMA (PB `docs/perf.md`) | Photo changes are well within one frame; the cursor must not force full blits |
| Palette writes | Only during vblank, or dots appear on screen (Maniac `lp_video.c:73-86`) | Palette swap and flip happen in the same vblank |
| Printer | `bios_print8bpp(buf, pal, 1)`. 8bpp with a 256-byte stride, 256×**241** dot window, 256-entry RGB555 palette which **must be in RAM** | The photo's own 256×240 pixels can be printed directly (last row duplicated) |
| Print return codes | 0 ok, 1 fail, 2 no cassette, 3 cancelled, 4 jam, 5 warm-up (retry while `bios_checkPrintTemp()`) | Copy `lp_print.c` retry logic and messages |
| Print side effects | Kills the pad scan (re-run `bios_vdpMode`, clear latches). Stops the ITU2 clock (`LP_ClockRearm`). Hangs held MIDI notes. Tramples the first 0x100 bytes of RAM | Reuse Maniac's `LP_InputPause/Rescan`, clock rearm and sound mute |
| Cassette | `bios_getSealType()`: 1 = XS-11/XS-14 (OK), 3 = XS-31 (refuse), 0 = none | Check before confirming a print |
| Input | `START 0x002, L 0x004, R 0x008, A 0x010, D 0x020, C 0x040, B 0x080, U/D/L/R 0x100-0x800`; decode `IO_GAMEPAD[0..1]` (not the low byte only) | Sampled in the ITU1 ISR with edge latching (PB `lp_input.c`) |
| ROM | Linked at `0x0E000000`, up to 4 MB in the linker script. Header 0x00, vectors 0x80, `.text` 0x480 | Pack budget ≈ 3.7 MB at 4 MB |
| Emulator | LoopyMSE writes `print_*.png`, but its hook clamps print height to 224 (`printer.cpp:180`) and always reports an XS-11 | Use LoopyMSE unmodified and accept 224 rows in emulator tests; the full print is verified on hardware |

---

## 3. Architecture

```
┌──────────────────────── Browser (GitHub Pages, static) ─────────────────────────┐
│  UI: add/reorder/crop photos, dither options, live Loopy preview, photo count   │
│        │                                                                        │
│        ▼  (Web Worker)                                                          │
│  core/ (pure TypeScript, no DOM — shared with the Node CLI)                     │
│    decode(adapter) → crop → resample (Lanczos/area) → quantize to RGB555        │
│    → dither → thumbnails → grid page composites → pack encoder                  │
│    → rompatch (truncate template, append, pad, header, checksum)                │
│        │                                                                        │
│        ▼                                                                        │
│  Blob download: MyPhotos.bin (+ optional byteswapped for MAME)                  │
└─────────────────────────────────────────────────────────────────────────────────┘
      ▲ fetched once, same-origin: template.bin (built by CI from cart/)

┌──────────── Cart (SH-1, C, Wonderful sh-elf-gcc 13.1, built in WSL/CI) ─────────┐
│  boot/ (header, vectors, crt0)    platform/ (video, input, clock, print, sound) │
│  src/pack.c   validate & index the pack at PACK_BASE                            │
│  src/image.c  load photo + palette into RAM for print/blit                      │
│  src/grid.c   show page composite, cursor on OBJ/BG layer, paging              │
│  src/viewer.c full-screen photo, prev/next                                      │
│  src/printui.c confirm → cassette check → "Printing…" → print → result          │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Key design decisions

1. **Photos are stored at 256×240, 8bpp, each with its own 256-colour palette.**
   The same buffer serves both the screen and the printer, so there is no
   runtime scaling and the sticker matches what's on screen.
2. **Grid pages are pre-rendered by the web app.** Each page is a full
   256×240 composite (thumbnails, header text, page number) with its own
   palette. The cart shows a page with a single blit.
   - The cart never composes thumbnails, so there are no palette clashes
     between photos and no text rendering on the cart.
   - The web app can use any font or language (Unicode titles and captions)
     because text is baked into the image.
   - The pack stores each page's cell rectangles, so the grid layout is a
     web-app setting and the cart doesn't hard-code it.
3. **Reserved UI palette slots.** The last 8 entries of every palette are
   fixed UI colours (black, white, greys, accent, shadow). Photo pixels never
   use them (dithering into UI colours shows as speckles), and slot 0 is
   black because it doubles as the backdrop. The cart can then draw the cursor,
   "Printing…" dialogs and error messages over any photo or page without
   touching the palette.
4. **The cursor lives on the OBJ (or BG tile) layer, not in the bitmap.**
   Moving it costs a few register writes per frame, not a blit, which gives a
   locked 60 fps grid (PB's approach: shadow copies in RAM, written during
   vblank). A prototype picks OBJ or BG, since the OBJ layer is limited to
   about 128 sprite pixels per scanline and a frame about 68 px wide fits.
5. **No compression; a fixed 54-photo limit.** Everything is stored raw, and
   the worst case (54 photos, 6 grid pages, print palettes) always fits in
   4 MB (budget in §8). There's no decompressor on the cart, and every dither
   mode, including Floyd–Steinberg, is equally cheap. The `codec` byte stays
   in the format (always 0) so compression could be added later without a
   format break.
6. **A separate print palette per photo (optional, phase 5).** The printer
   dyes don't look like a CRT. Since the BIOS prints through the palette, a
   print-tuned palette (gamma and saturation adjusted) costs only 512 bytes
   per photo and needs no second pixel buffer.
7. **Every sticker is filled edge to edge by default.** The crop box always
   has the printed sticker's exact proportions (calibrated in Phase 3, §8), so
   nothing prints blank unless the user opts in (Phase 4 crop rules).
   - **Portrait photos print as portrait stickers.** Cropping a 3:4 photo to
     a landscape sticker would throw away almost half of it. A tall photo is
     instead stored rotated 90° in the same 256×240 buffer, so it fills the
     sticker lengthwise. The `orientation` byte in the pack tells the cart.
     - *Printing:* the stored buffer is printed as-is, with no cart work.
     - *Screen:* the cart rotates it back upright into the RAM framebuffer,
       scaled to 240 px tall with black bars at the sides. That scaling also
       corrects for any difference between printer-dot and TV-pixel shape.
       It's one nearest-neighbour pass over 57 KB per photo change, so it's
       cheap.
     - *Grid:* the web app bakes an upright thumbnail into the page image.
   - Storage is the same for either orientation, so the 54-photo budget holds.
8. **Background music and two sound effects, integrated from
   LoopyPuzzleBobble.** Nothing is vendored. The useful parts of
   PuzzleBobble's hardware-proven sound code are adapted straight into
   `cart/platform/` and `cart/tools/`: `platform/lp_sound.c`,
   `platform/lp_sfx.c`, and the sequencer, effects engine and MIDI baker they
   build on. LoopySoundlib is unfinished; only its `docs/hardware.md` and
   `docs/instruments.md` are used, as reference.
   - **Hardware:** a uPD937 synth fed MIDI over SCI1. It has 4 channels with
     6/4/2/4 notes each, and 110 Casio presets that are **not** General MIDI.
     Velocity is on/off only, and sustain is the only controller that works.
   - **Channel plan:** music on console channels 0, 1 and 3 (6 + 4 + 4
     notes). Effects play alone on channel 2, so they never cut music notes.
   - **Music:** a public-domain piano piece whose *edition* is also public
     domain, taken from the Mutopia Project. `tools/lps_bake.py` bakes it into
     the template ROM as a looping song of about 2–3 KB.

     | Candidate | Length | Notes | Peak notes (treble / bass) | Edition licence |
     |---|---|---|---|---|
     | Satie, *Gymnopédie No. 1* (1888) | ~2:20 | 282 | 5 / 5 | Public Domain (CC0), Mutopia #37 |
     | Schumann, *Träumerei* Op. 15 No. 7 | ~1:50 | 339 | 4 / 5 | Public Domain (CC0), Mutopia #504 |

     Pick one after listening in LoopyMSE. Avoid CC-BY-SA editions (e.g.
     Mutopia's *Gnossienne No. 1*); that licence isn't GPL-2-compatible.
   - **Arrangement:**
     - The treble part goes to channel 0.
     - The bass peaks at 5 notes but channels 1 and 3 hold 4 each. A small
       pre-bake script splits it (lowest note to channel 3, the rest to
       channel 1) or trims overlapping held notes. `lps_bake.py --check-poly
       --simulate --strict` must pass.
     - The preset is a non-layered, piano-like one chosen by ear. Program 10
       is ruled out for melody because its high notes are clicks.
     - Sustain-pedal events can be added where chords should ring.
   - **Effects:** each is a hand-written table of 4-byte steps (soundlib has no
     effects-bank compiler). Every effect starts with a program change, which
     silences the channel, and a new effect cancels the previous one.
     - *Move tick* (D-pad, L/R page flips): program 10, note 96, the menu
       cursor click used by four retail Loopy games.
     - *Confirm* (A, B, Start): a short two-note rising chirp, program and
       notes chosen by ear.
   - **Behaviour:**
     - Music starts at boot and loops.
     - Before a print, all sounding notes are released, following LoopyManiac
       (`LP_SoundEnable(0)`). Music resumes when the print finishes.
     - C toggles music on and off; effects stay on. D opens the help
       popover.
   - **Tempo:** judge it with a stopwatch on hardware. LoopyMSE has run 7.5%
     slow in the past.

### Photo pack format v1 (big-endian, word-aligned)

```
PackHeader   @ PACK_BASE
  char[4]  magic        "LPCP"
  u16      version      1
  u16      flags        (bit0: has print palettes)
  u32      total_size
  u32      header_crc32 over [PACK_BASE+16, end of the tables) -- not the
                        images: a CRC of ~3.7 MB would take seconds at boot
                        on the SH-1, and the web app verifies the whole ROM
  u16      photo_count
  u16      page_count
  u32      photo_table  offset → PhotoEntry[photo_count]
  u32      page_table   offset → PageEntry[page_count]
  u32      meta_offset  offset → UTF-8 key/value strings (title, build date, tool version)

ImageRef (16 bytes)
  u16 width, u16 height         (256, 240)
  u8  codec (0 = raw), u8 reserved
  u16 palette_index             → palette pool (256 × u16 RGB555 each)
  u32 data_offset
  u32 data_length               (= width × height while codec is 0)

PhotoEntry
  ImageRef screen
  u16      print_palette_index  (0xFFFF = use screen palette)
  u8       orientation          (0 = landscape, 1 = portrait: stored rotated 90°)
  u8       reserved

PageEntry
  ImageRef composite
  u16      first_photo
  u8       cell_count
  u8       reserved
  CellRect[cell_count]          u8 x, u8 y, u8 w, u8 h  (cursor frame placement)
```

The pack format is specified once in `docs/pack-format.md`. The TS encoder and
C reader are both tested against the same golden fixtures.

---

## 4. Cart UX

**Boot:** a short splash (the pack title, pre-rendered on page 0 or its own
image), then grid page 1.

**Grid screen**

| Input | Action |
|---|---|
| D-pad | Move cursor; moving off the left or right edge flips to the previous or next page |
| L / R | Previous / next page (cursor keeps its cell, clamped on the last page) |
| A | Open the photo full screen |
| Start | Print the highlighted photo (confirm dialog) |
| C | Music on/off |
| D | Help popover |

**Full-screen viewer**

| Input | Action |
|---|---|
| D-pad left / right, L / R | Previous / next photo (wraps) |
| A or Start | Print (confirm dialog) |
| B | Back to the grid, with the cursor on the current photo (and its page) |
| C | Music on/off |
| D | Help popover |

**Print flow** (identical from both screens)

1. Show "Print this photo? A: Yes B: No". This is a small UI-colour box drawn
   on the bitmap page; the print buffer is a separate RAM buffer, so the box
   is never printed.
2. Check `bios_getSealType()`. Show "Insert a sticker cassette" if it returns
   0, or "This sticker type isn't supported" if it returns 3.
3. Draw "Printing…" and wait one vblank so it's actually on screen, because
   the print call blocks for seconds.
4. Pause input and mute sound, then call `bios_print8bpp` (retrying on code 5),
   then rearm the clock, rescan input and unmute.
5. Show the result message, then return to where the user was.

Button choices are defaults and easy to change in one table in
`src/input_map.h`.

---

## 5. Repository layout

```
loopy-photo-cart/
├─ .gitattributes          * text=auto eol=lf; binaries -text
├─ .editorconfig           end_of_line = lf
├─ CLAUDE.md               project rules (LF, reference projects, build tips)
├─ README.md
├─ COPYING.md              GPL v2 (project is GPL v2 or later)
├─ cart/                   Loopy program (C)
│  ├─ Makefile, Makefile.host
│  ├─ boot/  platform/  include/loopy/   ← adapted from PB/Maniac
│  ├─ src/                               ← pack, image, grid, viewer, printui
│  ├─ tools/loopy.ld, fixrom.py, mkfixture.py
│  ├─ host/                              ← host build: renders a pack to PNGs
│  └─ scripts/build.sh                   ← WSL build (rsync to ~/.cache like PB)
├─ web/                    Vite + TypeScript static site
│  ├─ src/core/            pure TS: resample, quantize, dither, pack, rompatch
│  ├─ src/worker/          Web Worker entry
│  ├─ src/ui/              app UI (no framework or a light one, e.g. Preact)
│  ├─ public/template/     template.bin (copied from cart build)
│  └─ test/                Vitest unit + golden tests
├─ cli/                    `node cli build demo/manifest.json -o demo.bin` (reuses web/src/core)
├─ sample-photos/          full-size originals (git-ignored, maintainer only)
├─ demo/
│  ├─ prepare_photos.py    originals → upright, sRGB, ≤1600 px, metadata-free
│  ├─ photos/              the cleaned demo photos (committed)
│  ├─ manifest.json        order, crops, orientation, dither, title
│  └─ README.md
├─ docs/
│  ├─ PLAN.md  pack-format.md  hardware-notes.md  testing.md
└─ .github/workflows/
   ├─ cart.yml             install Wonderful toolchain, build template, upload artifact
   ├─ web.yml              test + build site with the fresh template, deploy to Pages
   └─ release.yml          on tag: template.bin, demo ROM, site deploy
```

---

## 6. Phased milestones

### Phase 0 — Repo and toolchain skeleton ✅ (2026-09-13)
- Private repo, `.gitattributes` / `.editorconfig` enforcing LF, repo-local
  `core.autocrlf=false` (the system gitconfig has `autocrlf=true`).
- `cart/`:
  - Kasami's template boot, headers and linker skeleton.
  - A trimmed video backend (256×240 8bpp framebuffer, 8-row DMA bursts into
    two pages, palette and flip inside blanking), pad input, and the ITU1
    clock, all adapted from LoopyPuzzleBobble.
- Built with `cart\scripts\build.ps1`: git runs on Windows for the build id,
  the compiler runs in WSL.
- The ROM region ends at `PACK_OFFSET = 0x40000`, so code that grows into the
  pack fails to link. The template descriptor sits at ROM offset `0x20`
  (`cart/README.md`).
- On-screen text uses **Home Video Font** (GGBotNet, CC0,
  <https://ggbot.itch.io/home-video-font>): a monospaced 12×14 cell,
  uppercase, so ~18 characters a line inside the safe margins. UI copy must
  stay short.
- Print code (LoopyManiac's `lp_print.*`, input pause/rescan, clock rearm)
  moves to Phase 3, where it can be tested.
- **Exit met:** the template shows "No photos" in LoopyMSE. Test ROMs with a
  valid fake pack header and with a newer format version are detected
  correctly.

### Phase 1 — Pack format and ROM patching, end to end with fixtures ✅ (2026-09-13)
- `docs/pack-format.md` is the spec:
  - 40-byte header; CRC-32 over the header and tables only.
  - Photos 256×240 with their own palette. Slot 0 is black (it is also the
    backdrop) and slots 248–255 are reserved UI colours.
  - Portrait photos are stored rotated 90° clockwise.
  - Grid pages are optional (0 until Phase 2).
- `cart/tools/lpcpack.py` is the reference encoder and ROM patcher (standard
  library only). `lpcimage.py` does the crop, resize and RGB555 quantize with
  Pillow. `mkfixture.py` turns photo files into a test ROM.
- Cart:
  - full validation (`src/pack.c`);
  - full-screen viewer (`src/viewer.c`): Left/L and Right/R with wrap, a
    camcorder-style "3/25" counter for 1.5 s, and portrait photos shown
    upright with black bars.
- TS (`web/src/core`): `checksum.ts`, `crc32.ts`, `descriptor.ts`,
  `rompatch.ts`. `web/test` holds golden tests (Vitest) against files
  `cart/tools/mkgolden.py` generates, whose checksum is cross-checked with
  `fixrom.py`.
- **Exit met:** a 3-photo fixture (portrait first) and the 25-photo demo
  fixture both browse in LoopyMSE.
- Not done here: a C host build that parses TS-encoded packs, which needs the
  TS encoder (Phase 4). Photo change is a full ROM→RAM copy plus two page
  blits; faster paths are a Phase 5 item.

### Phase 2 — Grid and cursor ✅ in LoopyMSE (2026-09-13); hardware 60 fps check pending
- Grid pages are pre-rendered 256×240 images (`cart/tools/lpcimage.py`):
  - a header at y = 14: "PHOTOS" and "page/pages" in Home Video Font, with
    mixed-case control hints ("A: View", "D: Show Help") between them in Public Pixel Font
    (GGBotNet, CC0, <https://ggbot.itch.io/public-pixel-font>);
  - 3×3 64×60 thumbnails at x = 16/96/176, y = 34/100/166;
  - UI-slot background;
  - **no dithering** (at thumbnail size it left white specks).
- The pack format's page rules are tightened: page p holds photos 9p..., and
  cells are 64×60 with room for the cursor ring (`docs/pack-format.md`).
- The cursor is on the **OBJ0 sprite layer** (`cart/src/cursor.c`):
  - a 72×68 black/yellow/black ring from 8 sprites (one 32×32 corner tile
    flipped four ways plus two 8×8 edge tiles);
  - palette bank 15, so it uses UI slots 248–255 over any page;
  - at most 80 sprite pixels a scanline;
  - OAM is staged and published in the same blanking window as the page
    flip.
- Controls (`cart/src/grid.c`):
  - D-pad moves the cursor, repeating when held;
  - off the left or right edge turns the page, wrapping;
  - L/R turn the page, keeping the cell;
  - A opens the viewer; B there returns with the cursor on the photo last
    shown.
- Packs without pages still boot straight into the viewer.
- **Verified in LoopyMSE:**
  - 54 photos on 6 pages (3.98 MB ROM);
  - cursor moves, R paging, left-edge wrap to page 6;
  - A to open and B back to the right cell.
- **Pending:** the frame-rate check on hardware. Cursor moves should be a
  locked 60 fps (sprites only); a page turn is a ROM copy plus two blits.

### Phase 2b — Sound ✅ in LoopyMSE (2026-09-13); listening on hardware pending
- Integrate PuzzleBobble's sound code directly (no vendored library), set up
  as in its `platform/lp_sound.c`:
  - music on channels 0, 1 and 3 (mask 0x0B);
  - effects on channel 2 (mask 0x04);
  - ticked from our ITU1 interrupt.
- Fetch both candidate MIDIs into `cart/music/` with a `SOURCES.md` recording
  URL, edition and licence. Write the bass-split pre-pass. Bake both songs and
  listen in LoopyMSE, then choose one.
- Write the move-tick and confirm effect tables. Wire them to input, add the
  D music toggle, and release notes around printing.
- **Exit:** music loops cleanly for 10+ minutes with no stuck notes, and
  effects during dense passages don't drop music notes (check with LoopyMSE
  `--verbose` serial log).
- **Done:**
  - `cart/platform/lps/` is the sound driver as LoopyPuzzleBobble runs it;
    `cart/platform/lp_sound.c` is the glue (plan above, ticked from ITU1).
  - Music is *Gymnopédie No. 1* (Mutopia #37, public domain), split by
    `tools/sound/split_voices.py` and baked by `tools/sound/lps_bake.py` into
    `src/music_gymnopedie.h`: 2.3 KB, 9 bytes/s, notes at once 5/6, 4/4 and
    1/4, nothing dropped. `tools/sound/mkmusic.py` redoes both steps.
  - The piano is program 1, chosen from the instrument notes rather than by
    ear. *Träumerei* was not baked.
  - Effects on channel 2, program 10: a note-96 tick for d-pad movement and
    a rising 84→91 chirp for buttons.
  - **C** pauses and resumes the music (D became help). Printing pauses the
    music and silences effects.
  - Verified: LoopyMSE's serial log shows the program changes and the
    opening notes going out.
  - **Pending:** a listen on hardware (preset choice, loudness, effects over
    the music).
  - **Hardware result (2026-09-13):** music and effects were "flawless", but
    Gymnopédie was too melancholy. The music is now a playlist: Burgmüller's
    *La Candeur* (Mutopia #202), then Clementi's Sonatina Op. 36 No. 1
    (#804), repeating. Both are baked without looping, and `LP_SoundFrame()`
    starts the next song after a short gap. *La Pastorale* and Gymnopédie,
    or either playlist song alone, stay selectable with `-DLPC_MUSIC_*`. L/R in the viewer
    now make the d-pad tick, since they step one photo.

### Phase 3 — Printing
- Print flow and dialogs (§4), cassette check, input/clock/sound recovery.
- LoopyMSE is used unmodified. Its print PNGs stop at 224 rows, so emulator
  tests check only those rows; the full 241 rows are checked on hardware.
- Hardware test batch (stickers cost money, so plan one session):
  1. print from grid;
  2. print from viewer;
  3. no cassette;
  4. cancel mid-print;
  5. check input and clock still work afterwards;
  6. **measure the printed aspect ratio and orientation** to calibrate the web
     app's crop box (§8);
  7. print one portrait (rotated) photo to confirm which rotation direction
     reads naturally on the sticker.
- **Exit:** a real sticker matches the screen.
- **Implemented (2026-09-13), untested on hardware:**
  - `cart/platform/lp_print.c` follows LoopyManiac's hardware-proven sequence,
    from LoopyDOOM:
    - pause input, silence sound, zero the BIOS ISR work pointers;
    - call `bios_print8bpp`, retrying only warm-up;
    - rearm the ITU1 clock, re-arm the pad scan, and restore every VDP
      register (`LP_VideoRestore`).
  - `cart/src/printui.c` handles the flow:
    - a "PRINT PHOTO?" confirmation;
    - the cassette check (none / VHS refused);
    - "PRINTING..." shown on both pages before the blocking call;
    - a result message for each BIOS status.
  - The print buffer is the stored photo copied into RAM at 256×241, with the
    last row duplicated. It prints through the photo's print palette.
  - Start prints from the grid; A or Start prints from the viewer.
  - Verified in LoopyMSE with a boot-time test build
    (`EXTRA_CFLAGS=-DLPC_TEST_AUTOPRINT=n`): the emulator's print hook wrote
    the portrait photo, rotated to fill the sticker. The grid came back
    working afterwards.
  - The hardware test batch above (steps 1–7) is still to do.

### Phase 4 — Web app MVP ✅ (2026-09-13)

**As built** (`web/`, `docs/web.md`):
- **Core:** `web/src/core` is pure TypeScript shared by the worker and Node
  scripts:
  - Lanczos-3 resampling;
  - a deterministic median-cut RGB555 quantizer (slot rules as in
    `docs/pack-format.md`);
  - Floyd–Steinberg dithering for photos, none for grid pages;
  - grid pages from committed glyph tables (`cart/tools/mkwebfonts.py`);
  - the pack encoder, byte-identical to `lpcpack.py` (golden tests);
  - the ROM patcher, and a validator mirroring `cart/src/pack.c`.
- **Worker:** decodes one photo at a time with EXIF orientation, keeps a
  working copy of at most 1024 px, and reports errors per photo.
- **UI:**
  - add, drag-reorder and remove photos, with an "N / 54" counter;
  - a crop editor (drag, wheel or pinch zoom) with landscape/portrait,
    fill/whole photo (blurred fill) and dither;
  - a badge on crops that keep under 60% of the photo;
  - TV, Sticker and Grid previews of the exact bytes;
  - Build ROM, validated before download.
- **Privacy:** a CSP limits built pages to their own origin.
- **Verified:**
  - 28 Vitest tests, type check and build pass;
  - a headless Chromium smoke test (`npm run smoke`) builds a ROM with zero
    off-origin requests;
  - the Node-built ROM passes `cart/tools/lpcvalidate.py` and boots to the
    grid in LoopyMSE.
- **Deviations from the list below:**
  - a 1024 px working copy, not 1600;
  - an in-house detail-and-saturation auto-crop, not smartcrop.js, and no
    iPhone focus regions;
  - no WASM HEIC decoder (Safari only);
  - no project .zip save/load;
  - no OffscreenCanvas fallback.

**Original list:**
- Vite + TS. Add photos (file picker, drag & drop, multi-select), reorder,
  delete.
- **Accept photos straight off a phone, with no preparation by the user.** The
  app does in the browser what `demo/prepare_photos.py` does for the demo:
  - *Orientation:* decode with `createImageBitmap(file, {imageOrientation: 'from-image'})`.
  - *Colour:* the default `colorSpaceConversion` turns Display P3 or Adobe RGB
    into sRGB.
  - *Size:* 12–50 MP photos are decoded one at a time in the worker and
    immediately reduced to a working copy of 1600 px or less
    (`resizeWidth/Height`, `resizeQuality: 'high'`, with an OffscreenCanvas
    step-down fallback where those options aren't supported). The full-size
    bitmap is closed right away. Only the working copy and the original `File`
    handle (for re-cropping) are kept, so 54 huge photos stay within a few
    hundred MB.
  - *Metadata:* nothing to strip. Only quantized pixels go into the ROM, so
    EXIF and GPS data can never reach it. A test checks that the same pixels
    with and without metadata produce an identical ROM.
  - *Formats:* JPEG, PNG, WebP and AVIF in every current browser; HEIC natively
    in Safari. iPhones usually convert to JPEG when a browser picks from
    Photos. Elsewhere HEIC gets a clear message, and a lazy-loaded WASM HEIC
    decoder (e.g. libheif-js, LGPL, GPL-compatible) is a Phase 5 option.
  - *Failures* (corrupt file, CMYK JPEG, an out-of-memory decode) are reported
    per photo and never abort the batch.
- **Crop: fill the sticker by default.** No user should have to crop anything
  for a good result.
  - Every photo starts auto-cropped to the sticker's exact printed shape. Wide
    photos get landscape stickers, and tall photos get portrait (rotated)
    stickers (§3, decision 7).
  - The starting crop is chosen by content, not just centred. `smartcrop.js`
    (MIT, runs locally) scores detail and saturation so the subject isn't cut
    off. If the phone recorded focus or face regions (iPhone XMP regions, which
    the demo photos had), the crop keeps those regions inside it first. That
    metadata is read in the browser and never stored.
  - The user can drag to move the crop and scroll or pinch to zoom it. The crop
    can never extend past the photo's edges, so the sticker always stays full.
    They can also flip the sticker orientation or reset to the auto-crop.
  - Photos whose crop keeps less than about 60% of the picture (panoramas,
    very tall shots) get a badge in the photo list so they're easy to review.
  - "Fit whole photo" is an opt-in per photo. The leftover bands are filled
    with a blurred, enlarged copy of the photo (or a chosen solid colour), so
    nothing prints blank even then.
  - The preview shows the crop at its real printed shape, next to the TV view.
- In a Web Worker:
  - area/Lanczos downscale in linear light;
  - quantize directly in RGB555 space (Wu or k-means, e.g. via `image-q` or a
    small in-house implementation) with the 8 reserved UI colours pre-seeded;
  - dither modes: none, ordered (Bayer 4×4), Floyd–Steinberg;
  - pack encoding.
- Live **Loopy preview**: exactly the bytes that go into the ROM, rendered
  with approximate pixel aspect. Separate tabs for the grid page and the full
  screen.
- Photo counter ("37 / 54"); adding a 55th photo is refused. The format
  guarantees the ROM fits in 4 MB, and the size is still checked before
  download.
- "Build ROM" downloads a `.bin` generated in-memory.
- Privacy:
  - no network requests besides the site's own static assets;
  - a Content-Security-Policy `connect-src 'self'` meta tag;
  - no analytics;
  - a clear "your photos never leave your computer" note.
- Optional: save or load a project as a `.zip` (manifest + originals) so users
  can rebuild later.
- **Exit:** a ROM built in the browser boots in LoopyMSE and prints.

### Phase 5 — Quality and polish
- ✅ **Help popover on D** (`cart/src/help.c`, 2026-09-13; the grid header hints "D: Show Help"): a
  panel over the current screen listing every control, the website, and the
  copyright notice. Dismissed with D or B. Home Video Font for the heading
  and Public Pixel Font for the body; UI slots only, so it works over any
  page or photo.
- Print-palette tuning (after looking at real stickers), title and splash
  customization, optional captions baked into page composites.
- Fast paths: cache the neighbouring decoded photo in RAM so prev/next is
  instant, and optionally fade transitions.
- ✅ Service worker for offline use (`web/public/sw.js`: network first, cache
  fallback, same-origin only; registered in production builds).
- Not done, and why:
  - **print-palette tuning:** needs real stickers to compare;
  - **title/splash customization and captions:** optional; the grid header is
    fixed as "PHOTOS" by the user's choice;
  - **the neighbouring-photo cache:** unneeded, since page turns were quick
    on hardware.

### Phase 6 — Demo ROM and publishing
- `demo/manifest.json` over `demo/photos/`, which `demo/prepare_photos.py`
  produces from the git-ignored originals.
- `cli/` builds the demo ROM from the same TS core in CI. Output is
  deterministic, because decode goes through `sharp` → raw RGBA and everything
  after that is pure TS.
- `release.yml`: tag → GitHub Release with `template.bin`, `loopy-photo-cart-demo.bin`
  and the site deployed to Pages.
- Make the repo public at publish time (see §8, Pages on private repos).
- **Status (2026-09-13):**
  - **Done:**
    - `demo/manifest.json` with a fixed title, date and dither setting;
    - `cd web && npm run demo` builds `demo/build/loopy-photo-cart-demo.bin`
      with the web core, byte-reproducibly (Node's `jpeg-js` decodes; no
      separate `cli/`);
    - workflows: `.github/workflows/cart.yml` builds the template,
      `web.yml` tests, builds, smoke-tests and deploys Pages on `main`, and
      `release.yml` publishes the template, demo ROM and site zip on a `v*`
      tag.
  - The workflows are pushed and run on every push to `main`.
  - Pages is set to "GitHub Actions" with the custom domain
    **loopyphotocart.com** (user, 2026-09-13; HTTPS certificate approved).
    Pages works on the private repository, so `web.yml` deploys on every push
    to `main`. The cart's help popover shows the domain.
  - **Released (2026-09-13):**
    - the repository is public, with its homepage set to
      <https://loopyphotocart.com>;
    - tag `v1.0` published
      <https://github.com/techknight/loopy-photo-cart/releases/tag/v1.0>:
      `loopy-photo-cart-template.bin`, `loopy-photo-cart-demo.bin` (25
      photos, 3 pages, validated) and `loopy-photo-cart-site.zip`;
    - the playlist (*La Candeur* → Clementi → repeat) and the help popover's
      domain were checked in LoopyMSE before tagging.

---

## 7. Testing strategy

| Layer | How |
|---|---|
| Checksum / ROM patch | Vitest golden test: TS output must be byte-identical to `fixrom.py` on the same input |
| Pack format parity | TS encoder writes fixture packs; the C host build (`cart/host`, native gcc in WSL) parses them and dumps PNGs; CI compares against the expected pixels |
| Quantize/dither | Snapshot tests on small images; RGB555 invariants (all palette entries ≤ 0x7FFF, reserved slots fixed) |
| Cart UI | LoopyMSE with `scripts/capture.ps1`-style screenshot capture; key injection is unreliable, so use PB's `AUTOPILOT` build flag for scripted input |
| Printing | LoopyMSE `print_*.png` (patched clamp) plus the Phase 3 hardware batch |
| Web E2E | Playwright: load fixtures → build ROM → assert file size, header and checksum; and **assert zero non-same-origin network requests**. ✅ `web/scripts/smoke.mjs` (`npm run smoke`) |

---

## 8. Risks and open questions

1. **ROM size and photo limit: decided.** The target is the Floopy Drive, so
   the ROM is at most 4 MB (4,194,304 bytes). The grid is 3×3 with 64×60
   thumbnails (exactly ¼ scale), and a ROM holds at most 54 photos (6 pages).
   Worst-case budget, all uncompressed:

   | Part | Bytes |
   |---|---|
   | Code region (`PACK_BASE`) | 262,144 |
   | 54 photos × (57,600 pixels + 512 palette + 512 print palette) | 3,165,696 |
   | 6 grid pages × (57,600 pixels + 512 palette) | 348,672 |
   | Header and tables (generous) | 4,096 |
   | **Total** | **3,780,608** (about 400 KB spare) |
2. **Printed aspect and orientation: measured (2026-09-13).** The calibration
   stickers showed three things.
   - **Only 224 lines print.** The BIOS window is 256×241, but everything
     below line 223 is cut off (the chart's bottom border never printed).
   - **Dots aren't square:** 0.160 mm wide × 0.1425 mm tall (200 dots across
     = 32 mm, 200 lines = 28.5 mm). A 256×224 print is about 41.0 × 31.9 mm,
     shape 1.283 : 1 (`STICKER_ASPECT`).
   - **Portrait photos print rotated to the right**, as designed.

   Consequences, all implemented:
   - Photos are stored at 256×224 dots and printed line for line; grid pages
     stay 256×240.
   - Encoders crop to 1.283 : 1 and resample each axis separately.
   - The TV (square pixels, where the chart's circle was round) shows photos
     resampled to 256×200 letterboxed, or 187×240 for portrait.
   - The Python and web encoders and the cart agree (`docs/pack-format.md`).
   - The result becomes one constant, `STICKER_ASPECT`, in `web/src/core`.
     Until then 4:3 is the placeholder.
   - The crop box uses the physical sticker shape. If printer dots aren't
     square, the crop is resampled to 256×240 non-uniformly, so the sticker
     shows correct proportions.
3. **Emulator print clamp: decided, not patching.** LoopyMSE prints only
   224 rows. Emulator tests compare those rows; rows 224–240 are verified on
   hardware only.
4. **GitHub Pages on a private repo requires a paid plan** (Pro/Team). Develop
   privately with local `vite dev`, then flip the repo public when publishing.
   Alternatively keep the source private and deploy the site from a separate
   public repo.
5. **Wonderful toolchain in GitHub Actions.** It should work on ubuntu-latest
   via its bootstrap. Fallback: commit `web/public/template/template.bin`
   built locally, and have CI verify its descriptor version.
6. **License: decided, GPL v2 or later** (`COPYING.md`), matching the sibling
   Loopy projects.
   - ROMs built with the tool contain the GPL cart program plus the user's
     photos. The photos are data, not a derived work, so they stay the user's.
     Anyone sharing a built ROM satisfies the source requirement by pointing
     to this public repo.
   - Adapted third-party code gets credited in `NOTICE.md`. The print code
     reached us via LoopyManiac but comes from LoopyDOOM (ThroatyMumbo,
     GPL-2.0-or-later). Any boot/build skeleton from Kasami's
     loopy-homebrew-template keeps its zlib notice.
   - Third-party code must be GPL-2-compatible. MIT, BSD and zlib are fine
     (e.g. `image-q` and `smartcrop.js` are MIT). Apache-2.0 is **not** compatible
     with GPL-2.0-only, but it is with "or later" via GPLv3; still, avoid it.
7. **Demo photo privacy: handled.** Originals stay git-ignored in
   `sample-photos/`. Only `demo/prepare_photos.py` output is committed, and
   that script fails if any metadata survives. The tool itself never embeds
   metadata in ROMs.
8. **Overscan.** 240p on real TVs crops edges. Photos can go edge to edge, but
   grid UI, cursor and dialogs stay inside a ~16 px safe margin.

---

## 9. Project rules

- **LF line endings everywhere.** Enforced by `.gitattributes` and
  `.editorconfig`. Python and TS tools write files as bytes or with explicit
  `\n`.
- Run git from Git Bash/PowerShell, **not inside WSL**. WSL has no autocrlf,
  so the tree looks fully modified.
- Don't put heredocs inside `wsl -e bash -c "..."`; write a script file and run it.
- Reference projects:
  - `G:\LoopyManiac`: printing.
  - `G:\LoopyPuzzleBobble`: performance, video, input, capture tooling.
  - `G:\LoopySimCity`: do not use.
