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
// Constants shared by the whole cartridge program. The web app mirrors the
// ROM-layout ones (docs/PLAN.md, docs/pack-format.md).
//

#ifndef LPC_H
#define LPC_H

#include <stdint.h>

// Cartridge address space. The Floopy Drive takes at most 4 MB.
#define LPC_ROM_BASE   0x0E000000ul
#define LPC_ROM_LIMIT  (LPC_ROM_BASE + 0x400000ul)

// Where the photo pack starts. LPC_PACK_OFFSET comes from the Makefile, which
// hands the same value to the linker script.
#ifndef LPC_PACK_OFFSET
#error "LPC_PACK_OFFSET must be defined by the build"
#endif
#define LPC_PACK_BASE  (LPC_ROM_BASE + LPC_PACK_OFFSET)

// Version of the pack format this program reads, and of the descriptor that
// advertises it (src/descriptor.c).
#define LPC_PACK_VERSION 1
#define LPC_DESC_VERSION 1

#ifndef LPC_BUILD_ID
#define LPC_BUILD_ID "dev"
#endif

#endif
