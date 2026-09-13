# Notices

Loopy Photo Cart is licensed under the GNU General Public License, version 2
or later. The full text is in `COPYING.md`.

It is the author's own work, except for the third-party parts below.

## Kasami's loopy-homebrew-template (zlib)

[loopy-homebrew-template](https://github.com/kasamikona/loopy-homebrew-template)
supplies the cartridge program's base:
- the SH-1 boot code in `cart/boot/`;
- the Loopy hardware and BIOS headers in `cart/include/`;
- the linker scripts and `cart/tools/fixrom.py`.

Altered files are marked as such. The zlib notice is in
`cart/LICENSE-loopy-template.txt`.

## Fonts by GGBotNet (CC0 1.0)

Both fonts are dedicated to the public domain under Creative Commons Zero
v1.0 Universal.

- **Home Video Font**, from <https://ggbot.itch.io/home-video-font>: the main
  on-screen font. The font file and its licence are in
  `cart/assets/font/home-video/`, and `cart/src/font_homevideo.h` is generated
  from it.
- **Public Pixel Font**, from <https://ggbot.itch.io/public-pixel-font>: the
  small text on grid pages, such as control hints. The font file and its
  licence are in `cart/assets/font/public-pixel/`.

## References

LoopyDOOM (GPL-2.0-or-later) and the LoopyMSE emulator (GPL-3) were used as
references for hardware behaviour. No code was copied from LoopyMSE.
