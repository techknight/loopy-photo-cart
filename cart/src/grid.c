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
// The thumbnail grid.
//
// Each page is one pre-rendered image: moving between pages is a copy and a
// blit, and moving the cursor within a page is eight sprite entries.
//
//   D-pad   move the cursor (repeats when held); off the left or right edge
//           turns the page
//   L / R   previous / next page, keeping the cursor's cell where it exists
//   A       open the photo full screen; B there comes back to it
//

#include <string.h>

#include "loopy.h"

#include "lp_input.h"
#include "lp_video.h"

#include "cursor.h"
#include "grid.h"
#include "lpc.h"
#include "pack.h"
#include "viewer.h"

#define CELLS_PER_PAGE (LPC_GRID_COLS * LPC_GRID_ROWS)

// Held-direction repeat, in frames.
#define REPEAT_DELAY 20
#define REPEAT_EVERY 6

#define DPAD (GAMEPAD_BTN_UP | GAMEPAD_BTN_DOWN | GAMEPAD_BTN_LEFT | GAMEPAD_BTN_RIGHT)

static struct lpc_page page;
static unsigned page_index;
static unsigned page_count;
static unsigned cell;

static void ShowPage(unsigned index)
{
	page_index = index;
	LPC_PackPage(index, &page);
	memcpy(LP_Fb(), page.pixels, (size_t) LP_FB_W * LP_FB_H);
	LP_PalLoad(page.palette);
	LP_FbChanged();
	if (cell >= page.cell_count)
		cell = page.cell_count - 1u;
}

static void PlaceCursor(void)
{
	const struct lpc_cell *c = &page.cells[cell];

	LPC_CursorShowAround(c->x, c->y);
}

// New presses of the d-pad this frame, plus a repeat of the one being held.
static uint16_t DpadPresses(uint16_t edges, uint16_t held)
{
	static uint16_t repeating;
	static unsigned frames;

	if (edges & DPAD) {
		repeating = edges & DPAD;
		frames = 0;
		return repeating;
	}
	if (!(held & repeating)) {
		repeating = 0;
		return 0;
	}
	if (++frames >= REPEAT_DELAY &&
	    (frames - REPEAT_DELAY) % REPEAT_EVERY == 0)
		return (uint16_t) (held & repeating);
	return 0;
}

static void TurnPage(int delta, unsigned new_cell)
{
	unsigned next = (unsigned) ((int) page_index + delta + (int) page_count) % page_count;

	cell = new_cell;
	ShowPage(next);
}

static void Move(uint16_t press)
{
	unsigned col = cell % LPC_GRID_COLS;
	unsigned row = cell / LPC_GRID_COLS;
	unsigned count = page.cell_count;

	if (press & GAMEPAD_BTN_LEFT) {
		if (col > 0)
			--cell;
		else
			TurnPage(-1, row * LPC_GRID_COLS + (LPC_GRID_COLS - 1));
	} else if (press & GAMEPAD_BTN_RIGHT) {
		if (col + 1 < LPC_GRID_COLS && cell + 1 < count)
			++cell;
		else
			TurnPage(1, row * LPC_GRID_COLS);
	} else if (press & GAMEPAD_BTN_UP) {
		if (row > 0)
			cell -= LPC_GRID_COLS;
	} else if (press & GAMEPAD_BTN_DOWN) {
		// Onto a short last row, land on its last photo.
		if ((row + 1) * LPC_GRID_COLS < count)
			cell = cell + LPC_GRID_COLS < count ? cell + LPC_GRID_COLS
			                                     : count - 1;
	}
	PlaceCursor();
}

void LPC_GridRun(void)
{
	unsigned photo = 0;

	page_count = LPC_PackPageCount();
	LPC_CursorInit();

	for (;;) {
		// (Re)enter the grid on the page and cell holding `photo`.
		cell = photo % CELLS_PER_PAGE;
		ShowPage(photo / CELLS_PER_PAGE);
		PlaceCursor();

		for (;;) {
			uint16_t edges = LP_PadEdges();
			uint16_t press = DpadPresses(edges, LP_PadHeld());

			if (edges & GAMEPAD_BTN_A) {
				photo = page.first_photo + cell;
				break;
			}
			if (edges & GAMEPAD_BTN_LTRIG) {
				TurnPage(-1, cell);
				PlaceCursor();
			} else if (edges & GAMEPAD_BTN_RTRIG) {
				TurnPage(1, cell);
				PlaceCursor();
			} else if (press) {
				Move(press);
			}
			LP_VideoPresent();
		}

		LPC_CursorHide();
		photo = LPC_ViewerRun(photo, 1);
	}
}
