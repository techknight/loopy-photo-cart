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
// The sticker printer, through the boot BIOS.
//
// The print window is fixed by the BIOS: 256 dots wide by 241 lines, one byte
// per pixel at a 256-byte stride, resolved through a 256-entry RGB555 palette.
//

#ifndef __LP_PRINT_H__
#define __LP_PRINT_H__

#include <stdint.h>

#define LP_PRINT_W 256
#define LP_PRINT_H 241

// Result codes from the BIOS.
enum {
	LP_PRINT_OK        = 0,
	LP_PRINT_FAILED    = 1,
	LP_PRINT_NO_SEAL   = 2,
	LP_PRINT_CANCELLED = 3,
	LP_PRINT_JAM       = 4,
	LP_PRINT_HOT       = 5,
};

// bios_getSealType(): 0 = no cassette, 1 = the standard XS-11 (and what the
// XS-14 four-up reports), 3 = the XS-31 VHS spine cassette, which is refused.
#define LP_SEAL_NONE 0
#define LP_SEAL_VHS  3
int LP_PrintSealType(void);

// Print one LP_PRINT_W x LP_PRINT_H buffer of palette indices, which must be
// in work RAM, with `palette` (256 RGB555 entries; copied into RAM here).
// Blocks for the whole print -- seconds on hardware.
//
// The print disturbs the controller scan, the timer and the VDP mode; all
// three are restored before this returns, and the next LP_VideoPresent()
// re-publishes the framebuffer, palette and sprites.
int LP_PrintSticker(const uint8_t *pixels, const uint16_t *palette);

#endif
