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
// The photo pack appended at LPC_PACK_BASE (docs/pack-format.md).
//

#ifndef LPC_PACK_H
#define LPC_PACK_H

#include <stdint.h>

enum lpc_pack_status {
	LPC_PACK_OK,
	// The cartridge ends before the pack region: a bare template.
	LPC_PACK_ABSENT,
	// Something is there, but it is not a pack this program can read.
	LPC_PACK_BAD_MAGIC,
	LPC_PACK_BAD_VERSION,
	LPC_PACK_BAD_SIZE,
	LPC_PACK_BAD_CRC,
	LPC_PACK_BAD_TABLES,
};

struct lpc_photo {
	const uint8_t *pixels;         // LPC_IMAGE_W x LPC_IMAGE_H
	const uint16_t *palette;       // 256 entries, RGB555
	const uint16_t *print_palette; // 256 entries; the screen palette if none
	uint8_t orientation;           // LPC_ORIENT_*
};

// Find and validate the pack. Nothing past the cartridge header's own end
// address is read, so a bare template never reads beyond its data. The
// accessors below are only valid after this returns LPC_PACK_OK.
enum lpc_pack_status LPC_PackOpen(void);

unsigned LPC_PackPhotoCount(void);
void LPC_PackPhoto(unsigned index, struct lpc_photo *out);

// A short on-screen reason for a status (at most 18 characters).
const char *LPC_PackStatusText(enum lpc_pack_status status);

#endif
