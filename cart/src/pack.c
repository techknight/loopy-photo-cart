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
// Photo pack discovery. Phase 0 checks only the fixed header fields; the
// tables and images arrive with the pack format itself (docs/PLAN.md,
// Phase 1).
//

#include "lpc.h"
#include "pack.h"

// The cartridge header's second longword: the address of the last word the
// checksum covers, so the data ends two bytes after it (tools/fixrom.py).
#define ROM_HEADER_ROMLAST (*(const uint32_t *) (LPC_ROM_BASE + 4))

// Fixed start of every pack, big-endian.
struct pack_header {
	char     magic[4];     // "LPCP"
	uint16_t version;
	uint16_t flags;
	uint32_t total_size;   // bytes from LPC_PACK_BASE
	uint32_t header_crc32;
};

#define PACK_HEADER_SIZE ((uint32_t) sizeof(struct pack_header))

_Static_assert(sizeof(struct pack_header) == 16, "pack header is 16 bytes");

enum lpc_pack_status LPC_PackProbe(void)
{
	const struct pack_header *h = (const struct pack_header *) LPC_PACK_BASE;
	uint32_t rom_end = ROM_HEADER_ROMLAST + 2;

	// A bare template ends well below the pack region. Past its end the
	// cartridge reads back as open bus or a mirror of the start of the ROM,
	// neither of which may be mistaken for photos.
	if (rom_end < LPC_PACK_BASE + PACK_HEADER_SIZE)
		return LPC_PACK_ABSENT;

	if (h->magic[0] != 'L' || h->magic[1] != 'P' ||
	    h->magic[2] != 'C' || h->magic[3] != 'P')
		return LPC_PACK_BAD_MAGIC;

	if (h->version != LPC_PACK_VERSION)
		return LPC_PACK_BAD_VERSION;

	if (h->total_size < PACK_HEADER_SIZE ||
	    h->total_size > LPC_ROM_LIMIT - LPC_PACK_BASE ||
	    LPC_PACK_BASE + h->total_size > rom_end)
		return LPC_PACK_BAD_SIZE;

	return LPC_PACK_OK;
}

const char *LPC_PackStatusText(enum lpc_pack_status status)
{
	switch (status) {
	// At most 18 characters: one line of Home Video Font on screen.
	case LPC_PACK_OK:          return "PHOTOS FOUND";
	case LPC_PACK_ABSENT:      return "NO PHOTO PACK";
	case LPC_PACK_BAD_MAGIC:   return "UNKNOWN PACK";
	case LPC_PACK_BAD_VERSION: return "NEWER PACK FORMAT";
	case LPC_PACK_BAD_SIZE:    return "PACK IS DAMAGED";
	}
	return "UNKNOWN";
}
