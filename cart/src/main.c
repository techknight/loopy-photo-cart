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
// Loopy Photo Cart: entry point.
//
// Phase 0 boots, looks for a photo pack, and says what it found. The grid,
// the viewer and printing arrive in later phases (docs/PLAN.md).
//

#include <string.h>

#include "loopy.h"

#include "lp_clock.h"
#include "lp_input.h"
#include "lp_video.h"

#include "lpc.h"
#include "pack.h"
#include "text.h"

// UI palette slots. docs/PLAN.md reserves the top of every palette for these
// so dialogs can be drawn over any photo; the no-photos screen uses them too.
enum {
	COL_BG     = 0,
	COL_WHITE  = 255,
	COL_GREY   = 254,
	COL_DIM    = 253,
	COL_ACCENT = 252,
	COL_PANEL  = 251,
};

static void UiPalette(void)
{
	LP_PalSet(COL_BG,     RGB555(3, 4, 8));
	LP_PalSet(COL_WHITE,  RGB555(31, 31, 31));
	LP_PalSet(COL_GREY,   RGB555(22, 22, 24));
	LP_PalSet(COL_DIM,    RGB555(12, 12, 15));
	LP_PalSet(COL_ACCENT, RGB555(31, 14, 20));
	LP_PalSet(COL_PANEL,  RGB555(6, 7, 13));
}

static void FillRect(int x, int y, int w, int h, uint8_t colour)
{
	uint8_t *fb = LP_Fb();
	int row;

	for (row = y; row < y + h; ++row)
		memset(fb + row * LP_FB_W + x, colour, (size_t) w);
	LP_FbChanged();
}

static void ScreenNoPhotos(enum lpc_pack_status status)
{
	// A 16 px margin all round keeps everything inside a TV's overscan.
	FillRect(0, 0, LP_FB_W, LP_FB_H, COL_BG);
	FillRect(16, 40, LP_FB_W - 32, 140, COL_PANEL);
	FillRect(16, 40, LP_FB_W - 32, 2, COL_ACCENT);

	// Home Video's cell is 12x14, so a line holds 18 characters inside the
	// panel: keep every string here to that.
	LPC_TextDrawCentred(56, "LOOPY PHOTO CART", COL_ACCENT, 1);
	LPC_TextDrawCentred(92, "NO PHOTOS IN", COL_WHITE, 1);
	LPC_TextDrawCentred(110, "THIS CARTRIDGE", COL_WHITE, 1);
	LPC_TextDrawCentred(140, "ADD PHOTOS WITH", COL_GREY, 1);
	LPC_TextDrawCentred(158, "THE WEB APP", COL_GREY, 1);

	if (status != LPC_PACK_ABSENT)
		LPC_TextDrawCentred(186, LPC_PackStatusText(status), COL_ACCENT, 1);

	LPC_TextDrawCentred(206, LPC_BUILD_ID, COL_DIM, 1);
}

static void ScreenPackFound(void)
{
	FillRect(0, 0, LP_FB_W, LP_FB_H, COL_BG);
	LPC_TextDrawCentred(104, "PHOTOS FOUND", COL_WHITE, 1);
	LPC_TextDrawCentred(122, "VIEWER: PHASE 1", COL_GREY, 1);
}

int main(void)
{
	enum lpc_pack_status status;

	LP_VideoInit();
	LP_InputInit();
	LP_ClockInit();

	UiPalette();

	status = LPC_PackProbe();
	if (status == LPC_PACK_OK)
		ScreenPackFound();
	else
		ScreenNoPhotos(status);

	for (;;) {
		(void) LP_PadEdges();
		LP_VideoPresent();
	}
}
