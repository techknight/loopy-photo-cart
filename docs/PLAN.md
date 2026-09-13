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

The template carries a small **descriptor** at the start of its reserved
region: magic, pack-format version, `PACK_BASE` and build id. The web app reads
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
| Emulator | LoopyMSE writes `print_*.png`, but its hook clamps print height to 224 (`printer.cpp:180`) and always reports an XS-11 | Patch LoopyMSE-mouse, or accept 224 rows in emulator tests; real prints still need hardware |

---

## 3. Architecture

```
┌──────────────────────── Browser (GitHub Pages, static) ─────────────────────────┐
│  UI: add/reorder/crop photos, dither options, live Loopy preview, capacity bar  │
│        │                                                                        │
│        ▼  (Web Worker)                                                          │
│  core/ (pure TypeScript, no DOM — shared with the Node CLI)                     │
│    decode(adapter) → crop → resample (Lanczos/area) → quantize to RGB555        │
│    → dither → thumbnails → grid page composites → compress → pack encoder       │
│    → rompatch (truncate template, append, pad, header, checksum)                │
│        │                                                                        │
│        ▼                                                                        │
│  Blob download: MyPhotos.bin (+ optional byteswapped for MAME)                  │
└─────────────────────────────────────────────────────────────────────────────────┘
      ▲ fetched once, same-origin: template.bin (built by CI from cart/)

┌──────────── Cart (SH-1, C, Wonderful sh-elf-gcc 13.1, built in WSL/CI) ─────────┐
│  boot/ (header, vectors, crt0)    platform/ (video, input, clock, print, sound) │
│  src/pack.c   validate & index the pack at PACK_BASE                            │
│  src/codec.c  decompress into RAM (256×240 working buffer)                      │
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
   palette. The cart shows a page with one decompress and one blit.
   - The cart never composes thumbnails, so there are no palette clashes
     between photos and no text rendering on the cart.
   - The web app can use any font or language (Unicode titles and captions)
     because text is baked into the image.
   - The pack stores each page's cell rectangles, so the grid layout is a
     web-app setting and the cart doesn't hard-code it.
3. **Reserved UI palette slots.** The last 8 entries of every palette are
   fixed UI colours (black, white, greys, accent, shadow). The quantizer may
   still map photo pixels to them. The cart can then draw the cursor,
   "Printing…" dialogs and error messages over any photo or page without
   touching the palette.
4. **The cursor lives on the OBJ (or BG tile) layer, not in the bitmap.**
   Moving it costs a few register writes per frame, not a blit, which gives a
   locked 60 fps grid (PB's approach: shadow copies in RAM, written during
   vblank). A prototype picks OBJ or BG, since the OBJ layer is limited to
   about 128 sprite pixels per scanline and a frame about 60 px wide fits.
5. **Compression is per image, with a codec byte** (0 = raw, 1 = LZ4 block).
   The web app keeps whichever encoding is smaller. The on-cart LZ4 decoder is
   about 100 lines of C placed in RAM with `LP_RAMFUNC`. Floyd–Steinberg
   dithering compresses poorly and ordered dithering compresses well, so the
   capacity bar updates live as the user changes dither mode.
6. **A separate print palette per photo (optional, phase 5).** The printer
   dyes don't look like a CRT. Since the BIOS prints through the palette, a
   print-tuned palette (gamma and saturation adjusted) costs only 512 bytes
   per photo and needs no second pixel buffer.

### Photo pack format v1 (big-endian, word-aligned)

```
PackHeader   @ PACK_BASE
  char[4]  magic        "LPCP"
  u16      version      1
  u16      flags        (bit0: has print palettes)
  u32      total_size
  u32      crc32        over [PACK_BASE+16, PACK_BASE+total_size)
  u16      photo_count
  u16      page_count
  u32      photo_table  offset → PhotoEntry[photo_count]
  u32      page_table   offset → PageEntry[page_count]
  u32      meta_offset  offset → UTF-8 key/value strings (title, build date, tool version)

ImageRef (16 bytes)
  u16 width, u16 height         (256, 240)
  u8  codec, u8 reserved
  u16 palette_index             → palette pool (256 × u16 RGB555 each)
  u32 data_offset
  u32 data_length               (compressed size; decoded size = width × height)

PhotoEntry
  ImageRef screen
  u16      print_palette_index  (0xFFFF = use screen palette)
  u16      reserved

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

**Full-screen viewer**

| Input | Action |
|---|---|
| D-pad left / right, L / R | Previous / next photo (wraps) |
| A or Start | Print (confirm dialog) |
| B | Back to the grid, with the cursor on the current photo (and its page) |

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
├─ LICENSE                 (decide: MIT/zlib for tool + cart)
├─ cart/                   Loopy program (C)
│  ├─ Makefile, Makefile.host
│  ├─ boot/  platform/  include/loopy/   ← adapted from PB/Maniac
│  ├─ src/                               ← pack, codec, grid, viewer, printui
│  ├─ tools/loopy.ld, fixrom.py, mkfixture.py
│  ├─ host/                              ← host build: renders a pack to PNGs
│  └─ scripts/build.sh                   ← WSL build (rsync to ~/.cache like PB)
├─ web/                    Vite + TypeScript static site
│  ├─ src/core/            pure TS: resample, quantize, dither, lz4, pack, rompatch
│  ├─ src/worker/          Web Worker entry
│  ├─ src/ui/              app UI (no framework or a light one, e.g. Preact)
│  ├─ public/template/     template.bin (copied from cart build)
│  └─ test/                Vitest unit + golden tests
├─ cli/                    `node cli build demo/manifest.json -o demo.bin` (reuses web/src/core)
├─ demo/
│  ├─ photos/              cat photos (EXIF stripped!)
│  ├─ manifest.json        order, crops, dither, title
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

### Phase 0 — Repo and toolchain skeleton
- Private repo, `.gitattributes` / `.editorconfig` enforcing LF, repo-local
  `core.autocrlf=false` (the system gitconfig has `autocrlf=true`).
- Copy the minimal boot/platform/include tree from LoopyPuzzleBobble (video,
  clock, input) and LoopyManiac (`lp_print.*`, the input pause/rescan, clock
  rearm).
- The cart builds in WSL and boots in LoopyMSE to a solid-colour screen.
- `PACK_BASE` region, descriptor and linker `ASSERT` are in place.
- **Exit:** `make` produces a bootable `template.bin` showing "No photos".

### Phase 1 — Pack format and ROM patching, end to end with fixtures
- `docs/pack-format.md`.
- A Python fixture generator (`cart/tools/mkfixture.py`) turns 3 PNGs into a
  pack, raw codec only, to unblock cart work before the web app exists.
- Cart: pack validation, full-screen viewer with left/right navigation, page
  flip and palette-in-vblank.
- TS: `rompatch.ts` + `checksum.ts`, with a golden test against `fixrom.py`
  output.
- **Exit:** the fixture ROM browses 3 photos in LoopyMSE.

### Phase 2 — Grid and cursor
- Grid page composites (generated by the fixture tool at first), cell rects,
  OBJ/BG cursor, paging via L/R and edge wrap, grid↔viewer transitions keeping
  position.
- **Exit:** 30+ photos browse at a locked 60 fps (check `PROFILE=1`-style
  frame-time readout and the emulator).

### Phase 3 — Printing
- Print flow and dialogs (§4), cassette check, input/clock/sound recovery.
- LoopyMSE: patch the 224-row clamp in LoopyMSE-mouse so emulator prints show
  all 241 rows.
- Hardware test batch (stickers cost money, so plan one session):
  1. print from grid;
  2. print from viewer;
  3. no cassette;
  4. cancel mid-print;
  5. check input and clock still work afterwards;
  6. **measure the printed aspect ratio and orientation** to calibrate the web
     app's crop box (§8).
- **Exit:** a real sticker matches the screen.

### Phase 4 — Web app MVP
- Vite + TS. Add photos (file picker, drag & drop, multi-select), reorder,
  delete.
- Decode with `createImageBitmap(file, {imageOrientation: 'from-image'})` so
  EXIF rotation is respected. HEIC only works where the browser supports it,
  so show a friendly message otherwise.
- Per-photo crop editor: fixed target aspect, pan/zoom, with "fill" or "fit
  with border" (border colour selectable).
- In a Web Worker:
  - area/Lanczos downscale in linear light;
  - quantize directly in RGB555 space (Wu or k-means, e.g. via `image-q` or a
    small in-house implementation) with the 8 reserved UI colours pre-seeded;
  - dither modes: none, ordered (Bayer 4×4), Floyd–Steinberg;
  - LZ4 compression;
  - pack encoding.
- Live **Loopy preview**: exactly the bytes that go into the ROM, rendered
  with approximate pixel aspect. Separate tabs for the grid page and the full
  screen.
- Capacity bar against the selected cart size (see open questions).
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
- Print-palette tuning (after looking at real stickers), title and splash
  customization, optional captions baked into page composites.
- Grid layout options (3×3 or 4×3).
- Fast paths: cache the neighbouring decoded photo in RAM so prev/next is
  instant, and optionally fade transitions.
- Service worker for offline use.

### Phase 6 — Demo ROM and publishing
- `demo/manifest.json` + cat photos, **with EXIF/GPS stripped before commit**.
- `cli/` builds the demo ROM from the same TS core in CI. Output is
  deterministic, because decode goes through `sharp` → raw RGBA and everything
  after that is pure TS.
- `release.yml`: tag → GitHub Release with `template.bin`, `loopy-photo-cart-demo.bin`
  and the site deployed to Pages.
- Make the repo public at publish time (see §8, Pages on private repos).

---

## 7. Testing strategy

| Layer | How |
|---|---|
| Checksum / ROM patch | Vitest golden test: TS output must be byte-identical to `fixrom.py` on the same input |
| Pack format parity | TS encoder writes fixture packs; the C host build (`cart/host`, native gcc in WSL) parses them and dumps PNGs; CI compares against the expected pixels |
| LZ4 | Round-trip fuzz in TS; the host-built C decoder decodes TS-encoded blobs |
| Quantize/dither | Snapshot tests on small images; RGB555 invariants (all palette entries ≤ 0x7FFF, reserved slots fixed) |
| Cart UI | LoopyMSE with `scripts/capture.ps1`-style screenshot capture; key injection is unreliable, so use PB's `AUTOPILOT` build flag for scripted input |
| Printing | LoopyMSE `print_*.png` (patched clamp) plus the Phase 3 hardware batch |
| Web E2E | Playwright: load fixtures → build ROM → assert file size, header and checksum; and **assert zero non-same-origin network requests** |

---

## 8. Risks and open questions

1. **Target cartridge / flash hardware and max ROM size.** The linker allows
   4 MB, but what do your flash cart / Floopy Drive actually accept? This sets
   the capacity bar options.
   - Rough capacity at 4 MB: about 60 raw photos; with LZ4 and ordered dither,
     likely 90–120.
2. **Printed aspect and orientation.** LoopyMSE uses an 8:7 sticker aspect,
   Maniac framed at 16:15, and the XS-11 is 40×30 mm. Printing a calibration
   grid on hardware in Phase 3 settles the crop aspect for the web app.
3. **Emulator print clamp.** LoopyMSE prints only 224 rows. Patch our
   LoopyMSE-mouse copy, or live with it.
4. **GitHub Pages on a private repo requires a paid plan** (Pro/Team). Develop
   privately with local `vite dev`, then flip the repo public when publishing.
   Alternatively keep the source private and deploy the site from a separate
   public repo.
5. **Wonderful toolchain in GitHub Actions.** It should work on ubuntu-latest
   via its bootstrap. Fallback: commit `web/public/template/template.bin`
   built locally, and have CI verify its descriptor version.
6. **License.** The borrowed platform code is your own (Maniac is GPL only
   because of its ScummVM-derived interpreter). Pick a license for this repo
   (MIT or zlib is typical for a tool + template ROM).
7. **Demo photo privacy.** Phone photos carry GPS in EXIF, so strip it before
   committing. The tool itself never embeds metadata in ROMs.
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
