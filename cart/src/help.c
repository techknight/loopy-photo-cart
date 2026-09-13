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
// The help popover. Heading in Home Video Font, body in Public Pixel Font;
// UI slots only, so it reads the same over any page or photo.
//

#include "loopy.h"

#include "lp_input.h"
#include "lp_sound.h"
#include "lp_video.h"

#include "help.h"
#include "lpc.h"
#include "text.h"
#include "ui.h"

// Inside the TV-safe margins.
#define PANEL_X 16
#define PANEL_Y 16
#define PANEL_W (LP_FB_W - 2 * PANEL_X)
#define PANEL_H (LP_FB_H - 2 * PANEL_Y)
#define TEXT_X  (PANEL_X + 8)
#define KEY_W   (7 * LPC_SMALL_ADVANCE)
#define LINE_H  11

struct help_line {
	const char *key;
	const char *what;
};

// At most 25 characters of body per line: the panel is 208 px inside.
static const struct help_line controls[] = {
	{ "D-pad", "Move / next photo" },
	{ "A",     "Open photo" },
	{ "B",     "Back to the grid" },
	{ "L/R",   "Previous/next page" },
	{ "Start", "Print a sticker" },
	{ "C",     "Music on/off" },
	{ "D",     "Show/hide this help" },
};

#define CLOSE_KEYS (GAMEPAD_BTN_D | GAMEPAD_BTN_B | GAMEPAD_BTN_A)

void LPC_HelpShow(void)
{
	unsigned i;
	int y;

	// Modal: clear everything behind the panel, or the grid's header row
	// peeks out above it.
	LPC_FillRect(0, 0, LP_FB_W, LP_FB_H, LPC_UI_BG);
	LPC_FillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, LPC_UI_PANEL);
	LPC_FillRect(PANEL_X, PANEL_Y, PANEL_W, 2, LPC_UI_ACCENT);
	LPC_FillRect(PANEL_X, PANEL_Y + PANEL_H - 2, PANEL_W, 2, LPC_UI_ACCENT);

	LPC_TextDraw(LPC_FONT_LARGE, TEXT_X, PANEL_Y + 10, "HELP", LPC_UI_ACCENT);

	y = PANEL_Y + 34;
	for (i = 0; i < sizeof controls / sizeof controls[0]; ++i, y += LINE_H) {
		LPC_TextDraw(LPC_FONT_SMALL, TEXT_X, y, controls[i].key,
		             LPC_UI_HIGHLIGHT);
		LPC_TextDraw(LPC_FONT_SMALL, TEXT_X + KEY_W, y, controls[i].what,
		             LPC_UI_WHITE);
	}

	y += 8;
	LPC_TextDraw(LPC_FONT_SMALL, TEXT_X, y, "Make your own cartridge:",
	             LPC_UI_GREY);
	y += LINE_H;
	LPC_TextDraw(LPC_FONT_SMALL, TEXT_X, y, LPC_WEBSITE, LPC_UI_HIGHLIGHT);

	y += LINE_H + 8;
	LPC_TextDraw(LPC_FONT_SMALL, TEXT_X, y, LPC_COPYRIGHT, LPC_UI_DIM);
	y += LINE_H;
	LPC_TextDraw(LPC_FONT_SMALL, TEXT_X, y, "GPL-2.0-or-later", LPC_UI_DIM);

	// The same version as the web app's footer, on the panel's last line.
	LPC_TextDraw(LPC_FONT_SMALL, TEXT_X,
	             PANEL_Y + PANEL_H - 2 - 8 - LPC_SMALL_HEIGHT,
	             "Version " LPC_VERSION, LPC_UI_DIM);

	(void) LP_PadEdges();
	for (;;) {
		LP_VideoPresent();
		if (LP_PadEdges() & CLOSE_KEYS)
			break;
	}
	LP_SfxPlay(LP_SFX_BUTTON);
}
