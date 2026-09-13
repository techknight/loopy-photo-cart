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

## Home Video Font (CC0 1.0)

The on-screen font is Home Video Font by GGBotNet, from
<https://ggbot.itch.io/home-video-font>. It is dedicated to the public domain
under Creative Commons Zero v1.0 Universal. The font file
and its licence are in `cart/assets/font/`, and `cart/src/font_homevideo.h`
is generated from it.

## References

LoopyDOOM (GPL-2.0-or-later) and the LoopyMSE emulator (GPL-3) were used as
references for hardware behaviour. No code was copied from LoopyMSE.
