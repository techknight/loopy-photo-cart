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
// The template descriptor: 32 bytes at ROM offset 0x20, in the free space the
// cartridge header leaves before the vector table (tools/loopy.ld).
//
// The web app reads this before it appends a photo pack, and refuses a
// template whose pack format it does not write. That is what stops a stale
// cached template from silently producing a ROM that cannot read its photos.
//
// Layout, big-endian (the SH-1's native order):
//   0x20  char[4]  "LPCT"
//   0x24  u16      descriptor version
//   0x26  u16      pack format version the program reads
//   0x28  u32      pack base address
//   0x2C  u32      cartridge limit (end of the 4 MB window)
//   0x30  char[16] build id, NUL-padded
//

#include "lpc.h"

struct lpc_descriptor {
	char     magic[4];
	uint16_t desc_version;
	uint16_t pack_version;
	uint32_t pack_base;
	uint32_t rom_limit;
	char     build_id[16];
};

_Static_assert(sizeof(struct lpc_descriptor) == 32, "descriptor is 32 bytes");

__attribute__((section(".header.descriptor"), used))
const struct lpc_descriptor lpc_descriptor = {
	{ 'L', 'P', 'C', 'T' },
	LPC_DESC_VERSION,
	LPC_PACK_VERSION,
	LPC_PACK_BASE,
	LPC_ROM_LIMIT,
	LPC_BUILD_ID,
};
