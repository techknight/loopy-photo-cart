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
// Printing a photo as a sticker.
//
// The photo goes to the printer exactly as it is stored: a landscape photo as
// shown, a portrait one rotated so it fills the sticker lengthwise. Its print
// palette (the screen palette unless the pack carries a print-tuned one) is
// what the dots are resolved through. The dialog boxes are drawn over the
// framebuffer only; the printer never sees them.
//

#include <string.h>

#include "loopy.h"

#include "lp_input.h"
#include "lp_print.h"
#include "lp_sound.h"
#include "lp_video.h"

#include "lpc.h"
#include "pack.h"
#include "printui.h"
#include "text.h"
#include "ui.h"

// The dialog panel, inside the TV-safe margins.
#define BOX_X 24
#define BOX_Y 76
#define BOX_W (LP_FB_W - 2 * BOX_X)
#define BOX_H 88
#define TITLE_Y (BOX_Y + 22)
#define LINE_Y  (BOX_Y + 60)

// A result stays up this long unless dismissed.
#define RESULT_FRAMES 240

#define ANY_KEY (GAMEPAD_BTN_A | GAMEPAD_BTN_B | GAMEPAD_BTN_START)

// The BIOS reads the print window from work RAM: 256 x 241.
static uint8_t print_buf[LP_PRINT_W * LP_PRINT_H] __attribute__((aligned(4)));

static void Dialog(const char *title, uint8_t title_colour, const char *line)
{
	LPC_FillRect(BOX_X, BOX_Y, BOX_W, BOX_H, LPC_UI_PANEL);
	LPC_FillRect(BOX_X, BOX_Y, BOX_W, 2, LPC_UI_ACCENT);
	LPC_FillRect(BOX_X, BOX_Y + BOX_H - 2, BOX_W, 2, LPC_UI_ACCENT);
	LPC_TextDrawCentred(LPC_FONT_LARGE, TITLE_Y, title, title_colour);
	if (line)
		LPC_TextDrawCentred(LPC_FONT_SMALL, LINE_Y, line, LPC_UI_GREY);
}

// Present frames until one of `keys` is pressed or `frames` pass (0 = wait
// for a key). Returns the keys pressed, or 0 on timeout.
static uint16_t Wait(uint16_t keys, unsigned frames)
{
	unsigned n = 0;

	(void) LP_PadEdges();
	for (;;) {
		uint16_t e;

		LP_VideoPresent();
		e = LP_PadEdges() & keys;
		if (e) {
			LP_SfxPlay(LP_SFX_BUTTON);
			return e;
		}
		if (frames && ++n >= frames)
			return 0;
	}
}

static void ShowResult(int status)
{
	switch (status) {
	case LP_PRINT_OK:
		Dialog("PRINTED!", LPC_UI_HIGHLIGHT, "A: OK");
		break;
	case LP_PRINT_NO_SEAL:
		Dialog("NO CASSETTE", LPC_UI_ACCENT, "Insert a sticker cassette");
		break;
	case LP_PRINT_CANCELLED:
		Dialog("CANCELLED", LPC_UI_WHITE, "A: OK");
		break;
	case LP_PRINT_JAM:
		Dialog("STICKER JAM", LPC_UI_ACCENT, "Check the cassette");
		break;
	case LP_PRINT_HOT:
		Dialog("PRINTER TOO HOT", LPC_UI_ACCENT, "Wait a moment, try again");
		break;
	default:
		Dialog("PRINT FAILED", LPC_UI_ACCENT, "A: OK");
		break;
	}
	(void) Wait(ANY_KEY, RESULT_FRAMES);
}

void LPC_PrintPhoto(unsigned index, int confirm)
{
	struct lpc_photo photo;
	int seal;

	LPC_PackPhoto(index, &photo);

	if (confirm) {
		Dialog("PRINT PHOTO?", LPC_UI_WHITE, "A: Print   B: Cancel");
		if (!(Wait(ANY_KEY, 0) & (GAMEPAD_BTN_A | GAMEPAD_BTN_START)))
			return;
	}

	seal = LP_PrintSealType();
	if (seal == LP_SEAL_NONE) {
		ShowResult(LP_PRINT_NO_SEAL);
		return;
	}
	if (seal == LP_SEAL_VHS) {
		Dialog("WRONG CASSETTE", LPC_UI_ACCENT, "Use an XS-11 cassette");
		(void) Wait(ANY_KEY, RESULT_FRAMES);
		return;
	}

	// The print blocks for seconds, so the message must already be on the
	// visible page when it starts: two presents put it on both.
	Dialog("PRINTING...", LPC_UI_WHITE, "Please wait");
	LP_VideoPresent();
	LP_VideoPresent();

	memcpy(print_buf, photo.pixels, (size_t) LPC_IMAGE_W * LPC_IMAGE_H);
	memcpy(print_buf + LPC_IMAGE_W * LPC_IMAGE_H,
	       photo.pixels + LPC_IMAGE_W * (LPC_IMAGE_H - 1), LPC_IMAGE_W);

	ShowResult(LP_PrintSticker(print_buf, photo.print_palette));
}
