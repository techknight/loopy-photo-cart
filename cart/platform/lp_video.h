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
// Video backend: one 256x240 8bpp framebuffer in work RAM, shown through the
// bitmap layer's two VRAM pages.
//
// A frame is drawn into the framebuffer, DMA-copied into the page that is not
// being scanned out, and revealed by a single BM_SCROLLY store inside
// vertical blanking. The palette is staged in RAM and published in the same
// blanking window, so a photo and its palette always appear together.
//

#ifndef __LP_VIDEO_H__
#define __LP_VIDEO_H__

#include <stdint.h>

#define LP_FB_W 256
#define LP_FB_H 240

// Park a sprite fully off-screen in both axes. X-only hiding still spends the
// per-scanline OBJ fetch budget on real hardware (Y is evaluated before X).
// Hardware-verified in LoopyDOOM: X=384, Y=240.
#define LP_OAM_HIDDEN ((uint32_t) 0x00F00180u)

void LP_VideoInit(void);

// Rewrite every VDP register this backend owns, and re-publish the palette,
// sprites and both pages at the next present: bios_vdpMode() (called to
// re-arm the pad scan after a print) reprograms the VDP.
void LP_VideoRestore(void);

// The compose surface, LP_FB_W x LP_FB_H bytes, longword-aligned.
uint8_t *LP_Fb(void);

// Declare that the framebuffer changed, so both VRAM pages need it again.
void LP_FbChanged(void);

// Stage a palette entry (RGB555). Index 0 is transparent on the bitmap layer
// and shows the backdrop, which is kept equal to entry 0.
void LP_PalSet(unsigned idx, uint16_t rgb555);

// Stage all 256 entries at once (e.g. a photo's palette, straight from ROM).
void LP_PalLoad(const uint16_t *palette);

// --- Sprites (OBJ0) --------------------------------------------------------
//
// 4bpp sprites whose pixel values 1..15 show palette bank 15 (entries
// 241..255): values 8..15 are the reserved UI slots 248..255, so a sprite
// drawn in them looks the same over any photo or page. Char cells are 8x8,
// 32 bytes each (high nibble = even x); a 16x16 or 32x32 sprite with tile N
// reads cells N + row * 8 + col.

#define LP_OBJ_8X8   0u
#define LP_OBJ_16X16 1u
#define LP_OBJ_16X32 2u
#define LP_OBJ_32X32 3u

// 32-bit OAM entry (LoopyMSE render.cpp draw_obj): x b0-8, y high b9, size
// b10-11, palslot b12-13, x flip b14, y flip b15, y low b16-23, tile b24-31.
#define LP_OAM_ENTRY(x, y, size, xflip, yflip, tile)                          \
	((uint32_t) (((uint32_t) (x) & 0x1FF) |                               \
	             ((((uint32_t) (y) >> 8) & 1) << 9) |                     \
	             (((uint32_t) (size) & 3) << 10) |                        \
	             (((uint32_t) (xflip) & 1) << 14) |                       \
	             (((uint32_t) (yflip) & 1) << 15) |                       \
	             (((uint32_t) (y) & 0xFF) << 16) |                        \
	             (((uint32_t) (tile) & 0xFF) << 24)))

#define LP_OAM_SLOTS 128

// Stage an OAM slot; published inside the next present's blanking, never
// during active display (a mid-frame OAM store draws garbage on hardware).
void LP_OamSet(unsigned slot, uint32_t entry);
void LP_OamHide(unsigned slot);

// Copy `ncells` 32-byte char cells into sprite tile memory, starting at
// cell `cell`. Waits for vertical blank and writes inside it.
void LP_ObjCellsUpload(unsigned cell, const uint8_t *src, unsigned ncells);

// Show the framebuffer. Blits only while a page is stale, so a still screen
// costs one vsync a frame; always waits for the next vertical blank.
void LP_VideoPresent(void);

#endif
