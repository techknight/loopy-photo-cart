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
// Text on the framebuffer, in the two pixel fonts by GGBotNet (CC0 1.0):
//
//   LPC_FONT_LARGE  Home Video Font: monospaced 12x14 cell, uppercase only
//                   (lowercase draws as uppercase). About 18 characters fit a
//                   line inside the TV-safe margins.
//   LPC_FONT_SMALL  Public Pixel Font: 8x7 cell, mixed case. About 28 fit.
//
// ASCII 32-126; anything else draws as a space. A space straight after a
// colon is half a cell wide ("A: Print"), since a full monospaced gap reads
// as too wide there.
//

#ifndef LPC_TEXT_H
#define LPC_TEXT_H

#include <stdint.h>

enum lpc_font {
	LPC_FONT_LARGE,
	LPC_FONT_SMALL,
};

#define LPC_LARGE_ADVANCE 12
#define LPC_LARGE_HEIGHT  14
#define LPC_SMALL_ADVANCE 8
#define LPC_SMALL_HEIGHT  7

// Width in pixels of `s`.
int LPC_TextWidth(enum lpc_font font, const char *s);

// Draw `s` with its top-left at (x, y). Clipped to the framebuffer.
void LPC_TextDraw(enum lpc_font font, int x, int y, const char *s, uint8_t colour);

// Draw `s` centred horizontally on the screen.
void LPC_TextDrawCentred(enum lpc_font font, int y, const char *s, uint8_t colour);

#endif
