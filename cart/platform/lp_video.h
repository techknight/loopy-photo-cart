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

// The compose surface, LP_FB_W x LP_FB_H bytes, longword-aligned.
uint8_t *LP_Fb(void);

// Declare that the framebuffer changed, so both VRAM pages need it again.
void LP_FbChanged(void);

// Stage a palette entry (RGB555). Index 0 is transparent on the bitmap layer
// and shows the backdrop, which is kept equal to entry 0.
void LP_PalSet(unsigned idx, uint16_t rgb555);

// Show the framebuffer. Blits only while a page is stale, so a still screen
// costs one vsync a frame; always waits for the next vertical blank.
void LP_VideoPresent(void);

#endif
