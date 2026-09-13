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
// Opens the photo pack and goes to the grid (or straight to the viewer for a
// pack without grid pages). Without a usable pack it explains why instead.
//

#include "lp_clock.h"
#include "lp_input.h"
#include "lp_sound.h"
#include "lp_video.h"

#include "grid.h"
#include "lpc.h"
#include "pack.h"
#include "printui.h"
#include "text.h"
#include "ui.h"
#include "viewer.h"

static void ScreenNoPhotos(enum lpc_pack_status status)
{
	LPC_UiPalette();

	// A 16 px margin all round keeps everything inside a TV's overscan.
	// Slot 0 is the background, so a fill of 0 shows it via the backdrop.
	LPC_FillRect(0, 0, LP_FB_W, LP_FB_H, 0);
	LPC_FillRect(16, 40, LP_FB_W - 32, 140, LPC_UI_PANEL);
	LPC_FillRect(16, 40, LP_FB_W - 32, 2, LPC_UI_ACCENT);

	// Home Video's cell is 12x14, so a line holds 18 characters inside the
	// panel: keep every string here to that.
	LPC_TextDrawCentred(LPC_FONT_LARGE, 56, "LOOPY PHOTO CART", LPC_UI_ACCENT);
	LPC_TextDrawCentred(LPC_FONT_LARGE, 92, "NO PHOTOS IN", LPC_UI_WHITE);
	LPC_TextDrawCentred(LPC_FONT_LARGE, 110, "THIS CARTRIDGE", LPC_UI_WHITE);
	LPC_TextDrawCentred(LPC_FONT_SMALL, 142, "Add photos with the web app", LPC_UI_GREY);

	if (status != LPC_PACK_ABSENT && status != LPC_PACK_OK)
		LPC_TextDrawCentred(LPC_FONT_LARGE, 186, LPC_PackStatusText(status),
		                    LPC_UI_ACCENT);

	LPC_TextDrawCentred(LPC_FONT_SMALL, 210, LPC_BUILD_ID, LPC_UI_DIM);

	for (;;) {
		(void) LP_PadEdges();
		LP_VideoPresent();
	}
}

#ifdef LPC_TEST_AUTOPRINT
// Test builds only (EXTRA_CFLAGS=-DLPC_TEST_AUTOPRINT=n): print photo n-1
// without asking, shortly after boot, so the print path can be exercised in
// LoopyMSE (which writes the sticker as a PNG) without key injection.
static void TestAutoPrint(void)
{
	unsigned i;

	LPC_UiPalette();
	for (i = 0; i < 60; ++i)
		LP_VideoPresent();
	LPC_PrintPhoto(LPC_TEST_AUTOPRINT - 1, 0);
}
#endif

int main(void)
{
	enum lpc_pack_status status;

	LP_VideoInit();
	LP_InputInit();
	LP_SoundInit();
	LP_ClockInit();

	status = LPC_PackOpen();

#ifdef LPC_TEST_AUTOPRINT
	if (status == LPC_PACK_OK && LPC_PackPhotoCount() >= LPC_TEST_AUTOPRINT)
		TestAutoPrint();
#endif

	if (status == LPC_PACK_OK && LPC_PackPageCount() > 0)
		LPC_GridRun();
	if (status == LPC_PACK_OK && LPC_PackPhotoCount() > 0)
		(void) LPC_ViewerRun(0, 0);

	ScreenNoPhotos(status);
	return 0;
}
