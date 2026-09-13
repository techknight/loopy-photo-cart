//
// Copyright(C) 2026 Derek Quenneville
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// Constants shared by the whole cartridge program. The web app mirrors the
// ROM-layout ones (docs/pack-format.md).
//

#ifndef LPC_H
#define LPC_H

#include <stdint.h>

// Cartridge address space. The Floopy Drive takes at most 4 MB.
#define LPC_ROM_BASE   0x0E000000ul
#define LPC_ROM_LIMIT  (LPC_ROM_BASE + 0x400000ul)

// Where the photo pack starts. LPC_PACK_OFFSET comes from the Makefile, which
// hands the same value to the linker script.
#ifndef LPC_PACK_OFFSET
#error "LPC_PACK_OFFSET must be defined by the build"
#endif
#define LPC_PACK_BASE  (LPC_ROM_BASE + LPC_PACK_OFFSET)

// Version of the pack format this program reads, and of the descriptor that
// advertises it (src/descriptor.c).
#define LPC_PACK_VERSION 1
#define LPC_DESC_VERSION 1

#ifndef LPC_VERSION
#define LPC_VERSION "dev"
#endif

#ifndef LPC_BUILD_ID
#define LPC_BUILD_ID "dev"
#endif

// Shown in the help popover: the web app, on GitHub Pages under its own domain.
#define LPC_WEBSITE      "loopyphotocart.com"
#define LPC_COPYRIGHT    "(C) 2026 Derek Quenneville"

// Pack limits (docs/pack-format.md).
#define LPC_MAX_PHOTOS 54
#define LPC_GRID_COLS  3
#define LPC_GRID_ROWS  3
#define LPC_IMAGE_W    256
// A photo is stored at the printer's own resolution: the sticker shows 256
// dots by 224 lines (measured on hardware; the BIOS buffer is 241 lines but
// the rest is cut off). A grid page is a screen image, 256x240.
#define LPC_PHOTO_H    224
#define LPC_PAGE_H     240
#define LPC_THUMB_W    64
#define LPC_THUMB_H    60

// A printer dot is 0.160 mm wide and 0.1425 mm tall, so a stored photo is
// 1.283 times as wide as tall on the sticker. The TV's pixels are square, so
// the viewer shows that shape as 256x200 (landscape, letterboxed) or 187x240
// (portrait, pillarboxed).
#define LPC_TV_LANDSCAPE_H 200
#define LPC_TV_PORTRAIT_W  187

#define LPC_ORIENT_LANDSCAPE 0
#define LPC_ORIENT_PORTRAIT  1  // stored rotated 90 degrees clockwise

// Reserved UI palette slots, identical in every palette the pack carries, so
// UI can be drawn over any image (docs/pack-format.md).
enum {
	LPC_UI_BLACK = 248,
	LPC_UI_BG,
	LPC_UI_PANEL,
	LPC_UI_DIM,
	LPC_UI_GREY,
	LPC_UI_WHITE,
	LPC_UI_ACCENT,
	LPC_UI_HIGHLIGHT,
};

#endif
