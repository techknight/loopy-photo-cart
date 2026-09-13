# Notices

Loopy Photo Cart is licensed under the GNU General Public License, version 2
or later. The full text is in `COPYING.md`.

It is the author's own work, except for the third-party parts below.

## Kasami's loopy-homebrew-template (zlib)

[loopy-homebrew-template](https://github.com/kasamikona/loopy-homebrew-template)
supplies the cartridge program's base:
- the SH-1 boot code in `cart/boot/`;
- the Loopy hardware and BIOS headers in `cart/include/`;
- the linker scripts and `cart/tools/fixrom.py`;
- the SCI1 register setup in `cart/platform/lps/lps_port.h`.

Altered files are marked as such. The zlib notice is in
`cart/LICENSE-loopy-template.txt`.

## LoopyDOOM (GPL-2.0-or-later)

The sticker printing sequence in `cart/platform/lp_print.c` is adapted from
[LoopyDOOM](https://github.com/ThroatyMumbo/LoopyDOOM)'s
`platform/loopy_print.c` by ThroatyMumbo. That covers the BIOS call sequence,
its retry guards, and the zeroed ISR work pointers.

The sound driver in `cart/platform/lps/` is the author's own loopy-soundlib.
Two parts of it are also adapted from LoopyDOOM's
`platform/i_music_loopy.c`: the sequencer's event-stream design and the
sounding-note bitmaps. The music baker in `cart/tools/sound/lps_bake.py`
takes its set of options from LoopyDOOM's `tools/bake_music.py`.

## Fonts by GGBotNet (CC0 1.0)

Both fonts are dedicated to the public domain under Creative Commons Zero
v1.0 Universal.

- **Home Video Font**, from <https://ggbot.itch.io/home-video-font>: the main
  on-screen font. The font file and its licence are in
  `cart/assets/font/home-video/`, and `cart/src/font_homevideo.h` is generated
  from it.
- **Public Pixel Font**, from <https://ggbot.itch.io/public-pixel-font>: small
  text, such as control hints on grid pages and dialog lines. The font file
  and its licence are in `cart/assets/font/public-pixel/`, and
  `cart/src/font_publicpixel.h` is generated from it.

## Music (public domain)

The background music is J. F. F. Burgmüller's *La Candeur*, Op. 100 No. 1,
from [Mutopia Project](https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=202)
edition #202. Three alternatives are kept for auditioning:
- Burgmüller's *La Pastorale* (#218);
- Clementi's Sonatina Op. 36 No. 1 (#804);
- Satie's *Gymnopédie No. 1* (#37).

Every one of these compositions is in the public domain, and each edition was
placed in the public domain by its typesetter. The sources, and the steps that
turn them into `cart/src/music_*.h`, are in `cart/assets/music/`
(`SOURCES.md`).

## References

LoopyDOOM was also used as a reference for hardware behaviour. LoopyMSE, by
kasami (GPL-3), was used only as a reference for how the hardware behaves; no
code was copied from it.
