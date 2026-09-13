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
// Opens the photo pack and goes straight to the full-screen viewer. Without a
// usable pack it explains why instead. The grid and printing arrive in later
// phases (docs/PLAN.md).
//

#include "lp_clock.h"
#include "lp_input.h"
#include "lp_video.h"

#include "lpc.h"
#include "pack.h"
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
	LPC_TextDrawCentred(56, "LOOPY PHOTO CART", LPC_UI_ACCENT, 1);
	LPC_TextDrawCentred(92, "NO PHOTOS IN", LPC_UI_WHITE, 1);
	LPC_TextDrawCentred(110, "THIS CARTRIDGE", LPC_UI_WHITE, 1);
	LPC_TextDrawCentred(140, "ADD PHOTOS WITH", LPC_UI_GREY, 1);
	LPC_TextDrawCentred(158, "THE WEB APP", LPC_UI_GREY, 1);

	if (status != LPC_PACK_ABSENT && status != LPC_PACK_OK)
		LPC_TextDrawCentred(186, LPC_PackStatusText(status),
		                    LPC_UI_ACCENT, 1);

	LPC_TextDrawCentred(206, LPC_BUILD_ID, LPC_UI_DIM, 1);

	for (;;) {
		(void) LP_PadEdges();
		LP_VideoPresent();
	}
}

int main(void)
{
	enum lpc_pack_status status;

	LP_VideoInit();
	LP_InputInit();
	LP_ClockInit();

	status = LPC_PackOpen();
	if (status == LPC_PACK_OK && LPC_PackPhotoCount() > 0)
		LPC_ViewerRun(0);

	ScreenNoPhotos(status);
	return 0;
}
