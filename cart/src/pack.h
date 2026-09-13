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
// The photo pack appended at LPC_PACK_BASE.
//

#ifndef LPC_PACK_H
#define LPC_PACK_H

enum lpc_pack_status {
	LPC_PACK_OK,
	// The cartridge ends before the pack region: a bare template.
	LPC_PACK_ABSENT,
	// Something is there, but it is not a pack this program can read.
	LPC_PACK_BAD_MAGIC,
	LPC_PACK_BAD_VERSION,
	LPC_PACK_BAD_SIZE,
};

// Look for a pack without trusting anything past the cartridge header's own
// end address, so a bare template never reads beyond its data.
enum lpc_pack_status LPC_PackProbe(void);

// A short on-screen reason for a status (at most 18 characters).
const char *LPC_PackStatusText(enum lpc_pack_status status);

#endif
