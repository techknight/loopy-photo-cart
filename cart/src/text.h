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
// Text on the framebuffer, in Home Video Font (assets/font, CC0 1.0): a
// monospaced 12x14 cell, uppercase letters only (lowercase draws as
// uppercase). About 18 characters fit a line inside the TV-safe margins.
// ASCII 32-126; anything else draws as a space.
//

#ifndef LPC_TEXT_H
#define LPC_TEXT_H

#include <stdint.h>

#define LPC_TEXT_ADVANCE 12
#define LPC_TEXT_HEIGHT  14

// Width in pixels of `s` at `scale`.
int LPC_TextWidth(const char *s, int scale);

// Draw `s` with its top-left at (x, y). Clipped to the framebuffer.
void LPC_TextDraw(int x, int y, const char *s, uint8_t colour, int scale);

// Draw `s` centred horizontally on the screen.
void LPC_TextDrawCentred(int y, const char *s, uint8_t colour, int scale);

#endif
