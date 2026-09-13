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
// Shared declarations internal to the Casio Loopy platform backend.
//

#ifndef __LP_PLATFORM_H__
#define __LP_PLATFORM_H__

#include <stdint.h>

// Pixel loops that run from work RAM. The BIOS leaves the cartridge area at
// two wait states, so an opcode fetched from ROM costs three bus cycles to
// work RAM's one; the linker script carries .ramfunc inside .data and crt0
// copies it up with the initialised data. Put it on leaf loops only.
#define LP_RAMFUNC __attribute__((section(".ramfunc")))
#define LP_RAMLEAF __attribute__((section(".ramfunc"), noinline))

// Sample the gamepad. Called from the timer interrupt so that presses are
// still seen while the main loop is busy drawing.
void LP_PadSample(void);

// Mask interrupts around a read-modify-write of state the ITU1 handler also
// touches, or around a burst of VDP stores that must stay inside blanking.
static inline uint32_t LP_IrqBlock(void)
{
	uint32_t sr, masked;

	__asm__ __volatile__ ("stc sr,%0" : "=r" (sr));
	masked = sr | 0x000000F0;
	__asm__ __volatile__ ("ldc %0,sr" : : "r" (masked));

	return sr;
}

static inline void LP_IrqRestore(uint32_t sr)
{
	__asm__ __volatile__ ("ldc %0,sr" : : "r" (sr));
}

#endif
