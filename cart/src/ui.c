//
// Copyright(C) 2026 Derek Quenneville
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#include <string.h>

#include "loopy.h"

#include "lp_video.h"

#include "lpc.h"
#include "ui.h"

// Must match the reserved slot table in docs/pack-format.md: packs carry the
// same colours in every palette.
static const uint16_t ui_colours[8] = {
	RGB555(0, 0, 0),     // LPC_UI_BLACK
	RGB555(3, 4, 8),     // LPC_UI_BG
	RGB555(6, 7, 13),    // LPC_UI_PANEL
	RGB555(12, 12, 15),  // LPC_UI_DIM
	RGB555(22, 22, 24),  // LPC_UI_GREY
	RGB555(31, 31, 31),  // LPC_UI_WHITE
	RGB555(31, 14, 20),  // LPC_UI_ACCENT
	RGB555(31, 27, 8),   // LPC_UI_HIGHLIGHT
};

void LPC_UiPalette(void)
{
	unsigned i;

	for (i = 0; i < 8; ++i)
		LP_PalSet(LPC_UI_BLACK + i, ui_colours[i]);
	LP_PalSet(0, ui_colours[LPC_UI_BG - LPC_UI_BLACK]);
}

void LPC_FillRect(int x, int y, int w, int h, uint8_t colour)
{
	uint8_t *fb = LP_Fb();
	int row;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > LP_FB_W) w = LP_FB_W - x;
	if (y + h > LP_FB_H) h = LP_FB_H - y;
	if (w <= 0 || h <= 0)
		return;

	for (row = y; row < y + h; ++row)
		memset(fb + row * LP_FB_W + x, colour, (size_t) w);
	LP_FbChanged();
}
