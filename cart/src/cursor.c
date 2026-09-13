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
// The grid cursor.
//
// The ring is 72x68: one 32x32 corner tile used four times with the flip
// bits, and two 8x8 edge tiles closing the gaps between the corners.
//
//   slot  sprite         at            flips
//   0     corner 32x32   (0, 0)        -
//   1     corner         (40, 0)       x
//   2     corner         (0, 36)       y
//   3     corner         (40, 36)      x, y
//   4     top edge 8x8   (32, 0)       -
//   5     top edge       (32, 60)      y     (the bottom gap)
//   6     side edge 8x8  (0, 32)       -
//   7     side edge      (64, 32)      x     (the right gap)
//
// The busiest scanline carries 80 sprite pixels (two corners and both side
// edges), inside the hardware's 128-a-line budget.
//
// Ring colours, outside in: black, highlight, highlight, black -- sprite
// values 8 and 15, which bank 15 turns into UI slots 248 and 255.
//

#include <string.h>

#include "lp_video.h"

#include "cursor.h"

#define SPRITE_SLOT0  0
#define SPRITES       8

// Char cells: the corner's sixteen are cells 0-3, 8-11, 16-19 and 24-27
// (32x32 sprites read a 4x4 block on the 8-cell row stride); the edges take
// cells 4 and 5, outside that block.
#define TILE_CORNER   0
#define TILE_HEDGE    4
#define TILE_VEDGE    5
#define CELLS_USED    28

#define INK_BLACK     8
#define INK_HIGHLIGHT 15

static uint8_t cells[CELLS_USED * 32];

static unsigned RingInk(int depth)
{
	return (depth == 0 || depth == LPC_CURSOR_BORDER - 1) ? INK_BLACK
	                                                       : INK_HIGHLIGHT;
}

static void Plot(unsigned tile, int x, int y, unsigned ink)
{
	unsigned cell = tile + (unsigned) (y >> 3) * 8 + (unsigned) (x >> 3);
	unsigned byte = cell * 32 + (unsigned) (((y & 7) * 8 + (x & 7)) >> 1);

	if (x & 1)
		cells[byte] = (uint8_t) ((cells[byte] & 0xF0) | ink);
	else
		cells[byte] = (uint8_t) ((cells[byte] & 0x0F) | (ink << 4));
}

void LPC_CursorInit(void)
{
	int x, y;

	memset(cells, 0, sizeof cells);

	for (y = 0; y < 32; ++y) {
		for (x = 0; x < 32; ++x) {
			int dx = x < LPC_CURSOR_BORDER ? x : 99;
			int dy = y < LPC_CURSOR_BORDER ? y : 99;
			int depth = dx < dy ? dx : dy;

			if (depth < LPC_CURSOR_BORDER)
				Plot(TILE_CORNER, x, y, RingInk(depth));
		}
	}
	for (y = 0; y < LPC_CURSOR_BORDER; ++y)
		for (x = 0; x < 8; ++x)
			Plot(TILE_HEDGE, x, y, RingInk(y));
	for (y = 0; y < 8; ++y)
		for (x = 0; x < LPC_CURSOR_BORDER; ++x)
			Plot(TILE_VEDGE, x, y, RingInk(x));

	LP_ObjCellsUpload(0, cells, CELLS_USED);
	LPC_CursorHide();
}

void LPC_CursorShowAround(int tx, int ty)
{
	int x = tx - LPC_CURSOR_BORDER;
	int y = ty - LPC_CURSOR_BORDER;

	LP_OamSet(SPRITE_SLOT0 + 0, LP_OAM_ENTRY(x, y, LP_OBJ_32X32, 0, 0, TILE_CORNER));
	LP_OamSet(SPRITE_SLOT0 + 1, LP_OAM_ENTRY(x + 40, y, LP_OBJ_32X32, 1, 0, TILE_CORNER));
	LP_OamSet(SPRITE_SLOT0 + 2, LP_OAM_ENTRY(x, y + 36, LP_OBJ_32X32, 0, 1, TILE_CORNER));
	LP_OamSet(SPRITE_SLOT0 + 3, LP_OAM_ENTRY(x + 40, y + 36, LP_OBJ_32X32, 1, 1, TILE_CORNER));
	LP_OamSet(SPRITE_SLOT0 + 4, LP_OAM_ENTRY(x + 32, y, LP_OBJ_8X8, 0, 0, TILE_HEDGE));
	LP_OamSet(SPRITE_SLOT0 + 5, LP_OAM_ENTRY(x + 32, y + 60, LP_OBJ_8X8, 0, 1, TILE_HEDGE));
	LP_OamSet(SPRITE_SLOT0 + 6, LP_OAM_ENTRY(x, y + 32, LP_OBJ_8X8, 0, 0, TILE_VEDGE));
	LP_OamSet(SPRITE_SLOT0 + 7, LP_OAM_ENTRY(x + 64, y + 32, LP_OBJ_8X8, 1, 0, TILE_VEDGE));
}

void LPC_CursorHide(void)
{
	unsigned i;

	for (i = 0; i < SPRITES; ++i)
		LP_OamHide(SPRITE_SLOT0 + i);
}
