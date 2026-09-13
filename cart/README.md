# Cartridge program

The Casio Loopy side of Loopy Photo Cart. Building it produces
`build/loopy-photo-cart-template.bin`: the viewer with no photos in it. The web
app appends a photo pack at offset `0x40000` (see
[`../docs/pack-format.md`](../docs/pack-format.md)).

## Building

This needs the [Wonderful Toolchain](https://wonderful.asie.pl/) (`sh-elf-gcc`)
and `python3`.

On Linux:

```
bash cart/scripts/setup-toolchain.sh
make -C cart WONDERFUL_TOOLCHAIN="$HOME/wonderful"
```

On Windows, with the toolchain installed in WSL:

```
cart\scripts\build.ps1
```

This runs git on the Windows side to stamp the build id, then builds inside
WSL (`scripts/build.sh`) on a WSL-native copy of the tree.

The help screen shows the version from `web/package.json`, so the cartridge
and the website always agree.

## Running in LoopyMSE

Open any ROM in LoopyMSE. To take a screenshot from Windows:

```
cart\scripts\capture.ps1 -Rom cart\build\loopy-photo-cart-template.bin -Native -NativeScale 2 -Out shot.png
```

The script looks for LoopyMSE in an `emu` folder beside the repository, or
wherever `LOOPYMSE_PATH` points. That folder holds `LoopyMSE.exe`, its SDL
DLLs, `loopymse.ini` and the BIOS images. None of these are in the repository.

## Layout

| Path | What it is |
|---|---|
| `boot/`, `include/`, `tools/*.ld`, `tools/fixrom.py` | Kasami's loopy-homebrew-template (zlib) |
| `platform/` | Video, input, clock, printing and sound backends |
| `platform/lps/` | The sound driver: MIDI sequencer and sound effects |
| `src/main.c` | Boot, and the no-photos screen |
| `src/descriptor.c` | The 32-byte template descriptor at ROM offset `0x20` |
| `src/pack.c` | Photo pack discovery and validation |
| `src/grid.c`, `src/cursor.c` | Thumbnail grid pages and the sprite cursor ring |
| `src/viewer.c` | Full-screen photo viewer |
| `src/printui.c` | The print dialogs and sticker printing |
| `src/help.c` | The help popover |
| `src/ui.c`, `src/text.c` | Reserved UI colours, drawing and text |
| `src/font_*.h`, `src/music_*.h` | Generated font and music tables |
| `assets/font/` | Home Video Font and Public Pixel Font (CC0), with their licences |
| `assets/music/` | The music's public-domain sources (`SOURCES.md`) |
| `tools/lpcpack.py` | Reference pack encoder and ROM patcher (standard library only) |
| `tools/lpcimage.py` | Photo crop, resize and quantize for the fixture tools (Pillow) |
| `tools/lpcvalidate.py` | Checks a ROM the way the cartridge does |
| `tools/mkfixture.py` | Photos to a test ROM |
| `tools/mkcalibration.py` | A ROM of test charts for measuring printed stickers |
| `tools/mkfont.py`, `tools/mkwebfonts.py` | Generate the font tables for the cart and the web app |
| `tools/mkgolden.py`, `tools/mkgolden_web.py` | Regenerate `../web/test/fixtures` |
| `tools/sound/` | MIDI baker and music build scripts |

## Test ROMs

Build the template first. Then:

```
python cart\tools\mkfixture.py --out cart\build\fixture-demo.bin demo\photos\*.jpg
cart\scripts\capture.ps1 -Rom cart\build\fixture-demo.bin -Native -NativeScale 2 -Out shot.png
```

A fixture ROM boots into the grid. Pass `--no-grid` to boot straight into
the viewer instead.

| Screen | Input | Action |
|---|---|---|
| Grid | D-pad | Move the cursor (repeats when held). Off the left or right edge turns the page. |
| Grid | L / R | Previous / next page |
| Grid | A | Open the photo full screen |
| Grid | Start | Print the highlighted photo as a sticker |
| Viewer | Left / Right, L / R | Previous / next photo |
| Viewer | A / Start | Print this photo as a sticker |
| Viewer | B | Back to the grid, with the cursor on this photo |
| Both | C | Music on/off |
| Both | D | Help popover |

In LoopyMSE's default key map, Z is A, X is B, C is C, V is D, Q/W are L/R
and Enter is Start.

Test switches, set with `cart\scripts\build.ps1 EXTRA_CFLAGS=...`:

| Switch | Effect |
|---|---|
| `-DLPC_OSD_FRAMES=100000` | Keep the viewer's photo counter on screen |
| `-DLPC_TEST_AUTOPRINT=n` | Print photo n without asking, shortly after boot. LoopyMSE writes the sticker as a PNG in its working folder. |
| `-DLPC_TEST_HELP` | Open the help popover at boot |
| `-DLPC_MUSIC_PASTORALE`, `-DLPC_MUSIC_CLEMENTI`, `-DLPC_MUSIC_GYMNOPEDIE` | Play only that song, instead of the La Candeur → Clementi playlist |

Test switches change the template, so rebuild without them afterwards.

The viewer's "3/25" counter disappears after 1.5 s, usually before LoopyMSE
has finished starting. For screenshots, build with
`cart\scripts\build.ps1 EXTRA_CFLAGS=-DLPC_OSD_FRAMES=100000` to keep it on
screen.

After changing the template or the pack format, regenerate the web app's
golden files with `python cart\tools\mkgolden.py`, then run `npm test` in
`web\`.

## Template descriptor

The web app reads these 32 bytes at ROM offset `0x20` before appending a pack.
All values are big-endian.

| Offset | Type | Value |
|---|---|---|
| `0x20` | `char[4]` | `LPCT` |
| `0x24` | `u16` | Descriptor version (1) |
| `0x26` | `u16` | Pack format version the program reads (1) |
| `0x28` | `u32` | Pack base address (`0x0E040000`) |
| `0x2C` | `u32` | Cartridge limit (`0x0E400000`, 4 MB) |
| `0x30` | `char[16]` | Build id, NUL-padded |
