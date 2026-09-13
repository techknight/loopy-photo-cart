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
// Video backend. See lp_video.h for the model.
//
// Hardware rules this file follows, each learned on a console rather than in
// LoopyMSE (which models none of them):
//
//   - Palette RAM and OAM are the VDP's memory. A CPU store during active
//     display collides with the VDP's fetch and draws a row of random dots,
//     so both are written only inside blanking.
//   - OAM powers up full of DRAM garbage: every slot is parked at init.
//   - bios_vdpMode() reprograms the VDP, so it is called before any other
//     register is written, never after.
//   - BM_SUBPAL offsets every bitmap index and powers up undefined, so it is
//     written explicitly.
//

#include <string.h>

#include "loopy.h"

#include "lp_platform.h"
#include "lp_video.h"

// BM_MODE_8BPP_SHARED maps all 128 KB of bitmap VRAM as one 256x512 canvas:
// two 240-line pages, one at canvas row 0 and one at row 256.
#define PAGE_STRIDE_BYTES 0x10000u
#define PAGE_SCROLLY(p)   ((uint16_t) ((p) ? 256u : 0u))

// SH7021 DMAC channel 0: enabled, 16-bit transfers to match the VDP bus,
// burst mode, auto-request, both addresses incrementing. Bit 2 (interrupt
// enable) stays clear -- LoopyMSE asserts on a transfer with it set.
#define DMAC_CHCR_BLIT 0x5C19
#define DMAC_CHCR_TE   0x0002
#define DMA_SPIN_MAX   100000u

// Burst mode holds the bus until the count expires, so a whole frame in one
// burst would stall the 2 ms ITU1 tick for ~8 ms. Eight rows is 1024 words,
// well inside a tick, and 240 divides by 8.
#define BLIT_ROWS_PER_BURST 8

// With BG_CTRL = 0 the tilemaps take the first 0x4000 bytes of tile VRAM
// (two 64x64 maps), so char data starts at word 0x2000 (LoopyMSE render.cpp
// get_tilemap_info; the same constant LoopyManiac's cursor uses on hardware).
#define CHAR_DATA_W  0x2000u
#define TILE_VRAM_W  0x8000u

static uint8_t framebuffer[LP_FB_W * LP_FB_H] __attribute__((aligned(4)));
static uint16_t pal_shadow[256];
static int pal_dirty;
static uint32_t oam_shadow[LP_OAM_SLOTS];
static int oam_dirty;
static int back_page;
// How many VRAM pages still hold an older framebuffer: 2 after a change, and
// one fewer per present.
static int stale_pages;

static void BlitToPage(int page)
{
	const uint8_t *src = framebuffer;
	uint32_t dst = (uint32_t) &VDP.BITMAP_VRAM_8BIT[page * PAGE_STRIDE_BYTES];
	int left = LP_FB_H;

	while (left > 0) {
		int rows = left < BLIT_ROWS_PER_BURST ? left : BLIT_ROWS_PER_BURST;
		uint32_t spin = 0;

		DMAC_SAR0 = (uint32_t) src;
		DMAC_DAR0 = dst;
		DMAC_TCR0 = (uint16_t) (rows * (LP_FB_W / 2));
		DMAC_CHCR0 = DMAC_CHCR_BLIT; // writing DE=1 kicks the burst

		// Bounded: a torn frame is a better failure than a dead console.
		while (!(DMAC_CHCR0 & DMAC_CHCR_TE) && spin < DMA_SPIN_MAX)
			++spin;

		src += rows * LP_FB_W;
		dst += (uint32_t) (rows * LP_FB_W);
		left -= rows;
	}
}

void LP_VideoInit(void)
{
	unsigned i;

	// Blank the display while reprogramming it.
	VDP.SCREENPRIO = 0;
	VDP.BACKDROP_A = 0;
	VDP.BACKDROP_B = 0;

	// The BIOS wants a few fields with scanning off to detect the device.
	bios_vdpMode(CONTROL_MODE_NONE, 0);

	for (i = 0; i < LP_OAM_SLOTS; ++i) {
		oam_shadow[i] = LP_OAM_HIDDEN;
		VDP.OAM[i] = LP_OAM_HIDDEN;
	}
	oam_dirty = 0;

	// Tile VRAM powers up as DRAM garbage too; clear it while the display
	// is blanked, so no sprite ever fetches a cell nothing uploaded.
	for (i = 0; i < TILE_VRAM_W; ++i)
		VDP.TILE_VRAM[i] = 0;

	// Mode first: it tramples the registers below if they are written
	// before it.
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_240P);

	VDP.BG_CTRL = 0;
	VDP.CHARBASE = 0;
	// One OBJ engine (id offset 0: every slot is OBJ0), 4bpp, char window 0,
	// and every palslot on palette bank 15 -- the UI slots.
	VDP.OBJ_CTRL = OBJ_FORMAT_4BPP;
	VDP.OBJ_SUBPAL[0] = OBJ_SUBPAL(15, 15, 15, 15);
	VDP.OBJ_SUBPAL[1] = OBJ_SUBPAL(15, 15, 15, 15);

	memset(framebuffer, 0, sizeof framebuffer);
	memset(pal_shadow, 0, sizeof pal_shadow);
	for (i = 0; i < 0x100; ++i)
		VDP.PALETTE[i] = 0;
	pal_dirty = 0;

	VDP.BM_SCROLLX[0] = 0;
	VDP.BM_SCROLLY[0] = PAGE_SCROLLY(0);
	VDP.BM_SCREENX[0] = 0;
	VDP.BM_SCREENY[0] = 0;
	VDP.BM_WIDTH[0] = LP_FB_W - 1;
	VDP.BM_HEIGHT[0] = LP_FB_H - 1;
	VDP.BM_CTRL = BM_MODE_8BPP_SHARED;
	VDP.BM_SUBPAL = BM_SUBPAL(0, 0, 0, 0);

	sys_setDmaEnabled(true);

	// Prime both pages so the first flip cannot expose power-up VRAM.
	BlitToPage(0);
	BlitToPage(1);
	back_page = 1;
	stale_pages = 0;

	VDP.BLEND = BLEND_MATH;
	VDP.SCREENPRIO = SCREEN_A_ENABLE | PRIORITY_BM_A | PRIORITY_BG0_A
	               | PRIORITY_OBJ0_A;
	VDP.LAYER_CTRL = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A,
	                              LAYER_SCREEN_A, LAYER_SCREEN_A)
	               | LAYER_ENABLE_BM0 | LAYER_ENABLE_OBJ0;
}

void LP_VideoRestore(void)
{
	VDP.BG_CTRL = 0;
	VDP.CHARBASE = 0;
	VDP.OBJ_CTRL = OBJ_FORMAT_4BPP;
	VDP.OBJ_SUBPAL[0] = OBJ_SUBPAL(15, 15, 15, 15);
	VDP.OBJ_SUBPAL[1] = OBJ_SUBPAL(15, 15, 15, 15);

	VDP.BM_SCROLLX[0] = 0;
	VDP.BM_SCROLLY[0] = PAGE_SCROLLY(back_page ^ 1);
	VDP.BM_SCREENX[0] = 0;
	VDP.BM_SCREENY[0] = 0;
	VDP.BM_WIDTH[0] = LP_FB_W - 1;
	VDP.BM_HEIGHT[0] = LP_FB_H - 1;
	VDP.BM_CTRL = BM_MODE_8BPP_SHARED;
	VDP.BM_SUBPAL = BM_SUBPAL(0, 0, 0, 0);

	sys_setDmaEnabled(true);

	VDP.BLEND = BLEND_MATH;
	VDP.SCREENPRIO = SCREEN_A_ENABLE | PRIORITY_BM_A | PRIORITY_BG0_A
	               | PRIORITY_OBJ0_A;
	VDP.LAYER_CTRL = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A,
	                              LAYER_SCREEN_A, LAYER_SCREEN_A)
	               | LAYER_ENABLE_BM0 | LAYER_ENABLE_OBJ0;

	pal_dirty = 1;
	oam_dirty = 1;
	stale_pages = 2;
}

void LP_OamSet(unsigned slot, uint32_t entry)
{
	if (slot < LP_OAM_SLOTS && oam_shadow[slot] != entry) {
		oam_shadow[slot] = entry;
		oam_dirty = 1;
	}
}

void LP_OamHide(unsigned slot)
{
	LP_OamSet(slot, LP_OAM_HIDDEN);
}

void LP_ObjCellsUpload(unsigned cell, const uint8_t *src, unsigned ncells)
{
	volatile uint16_t *d = &VDP.TILE_VRAM[CHAR_DATA_W + cell * 16];
	unsigned words = ncells * 16;
	uint32_t sr;
	unsigned i;

	bios_vsync();
	sr = LP_IrqBlock();
	for (i = 0; i < words; ++i)
		d[i] = (uint16_t) ((src[2 * i] << 8) | src[2 * i + 1]);
	LP_IrqRestore(sr);
}

uint8_t *LP_Fb(void)
{
	return framebuffer;
}

void LP_FbChanged(void)
{
	stale_pages = 2;
}

void LP_PalSet(unsigned idx, uint16_t rgb555)
{
	if (idx < 256 && pal_shadow[idx] != rgb555) {
		pal_shadow[idx] = rgb555;
		pal_dirty = 1;
	}
}

void LP_PalLoad(const uint16_t *palette)
{
	memcpy(pal_shadow, palette, sizeof pal_shadow);
	pal_dirty = 1;
}

void LP_VideoPresent(void)
{
	int flip = stale_pages > 0;
	uint32_t sr;
	unsigned i;

	// Build into the page the VDP is not scanning out, during active
	// display; that is the point of having two pages.
	if (flip)
		BlitToPage(back_page);

	// bios_vsync() is the frame sync that works on real hardware -- the
	// vblank interrupt does not fire there.
	bios_vsync();

	// Inside blanking. Interrupts are held off so the ITU1 tick cannot push
	// the tail of the palette burst out into active display.
	sr = LP_IrqBlock();

	if (pal_dirty) {
		for (i = 0; i < 0x100; ++i)
			VDP.PALETTE[i] = pal_shadow[i];
		VDP.BACKDROP_A = pal_shadow[0];
		pal_dirty = 0;
	}

	// Sprites move in the same window as the flip, so the cursor and the
	// page it sits on always change together.
	if (oam_dirty) {
		for (i = 0; i < LP_OAM_SLOTS; ++i)
			VDP.OAM[i] = oam_shadow[i];
		oam_dirty = 0;
	}

	// The flip: one 16-bit store. The VDP re-reads this register every
	// scanline, so a store outside blanking is a raster split, not a flip.
	if (flip)
		VDP.BM_SCROLLY[0] = PAGE_SCROLLY(back_page);

	LP_IrqRestore(sr);

	if (flip) {
		back_page ^= 1;
		--stale_pages;
	}
}
