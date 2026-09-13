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

static uint8_t framebuffer[LP_FB_W * LP_FB_H] __attribute__((aligned(4)));
static uint16_t pal_shadow[256];
static int pal_dirty;
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

	for (i = 0; i < 0x80; ++i)
		VDP.OAM[i] = LP_OAM_HIDDEN;

	// Mode first: it tramples the registers below if they are written
	// before it.
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_240P);

	VDP.BG_CTRL = 0;
	VDP.OBJ_CTRL = OBJ_FORMAT_4BPP;
	VDP.CHARBASE = 0;

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
	               | LAYER_ENABLE_BM0;
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
