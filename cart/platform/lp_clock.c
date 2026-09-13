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
// Free-running millisecond clock, from an SH7021 ITU channel.
//
// Counting vblanks only advances when something calls bios_vsync(), so the
// CPU time spent drawing is invisible to such a clock. A hardware counter
// does not care what the CPU is doing.
//
// Channel choice: ITU0 belongs to the BIOS, which drives BGM playback from it
// (bios_isrAutoUpdateBgm sits on its compare-match vector). ITU0 and ITU1
// priorities share IPRC, the only priority register LoopyMSE implements, so
// channel 1 is the one that works on both the emulator and the console.
//
// At F_CPU/8 = 2 MHz the 16-bit counter wraps every 32.768 ms, less than a
// slow frame, so the counter is cleared on compare match and the handler
// counts matches rather than the main loop polling it.
//

#include "loopy.h"

#include "lp_clock.h"
#include "lp_platform.h"
#include "lp_sound.h"

#define CLOCK_HZ     (F_CPU / 8)
#define TICK_MS      LP_CLOCK_TICK_MS
#define TICK_COUNTS  (CLOCK_HZ / (1000 / TICK_MS))

#if TICK_COUNTS > 65535
#error "TICK_COUNTS does not fit in GRA1"
#endif

// Sample the pad on every fifth 2 ms tick: 100 Hz, a rate proven on
// hardware.
#define PAD_DIV      5

#define ITU1_START   0x02 // TSTR bit 1

#define TCR_CLEAR_ON_GRA 0x20 // CCLR = 01
#define TCR_CLOCK_DIV8   0x03 // CKS  = 11
#define TIER_IMIEA       0x01
#define TSR_IMFA         0x01

static volatile uint32_t clock_ticks; // TICK_MS units since init

__attribute__((interrupt_handler))
void LP_ClockISR(void)
{
	static uint8_t pad_div;

	// The flag must be read as 1 before it can be written back as 0.
	(void) ITU_TSR1;
	ITU_TSR1 = (uint8_t) ~TSR_IMFA;

	++clock_ticks;

	LP_SoundTick();

	// The pad is sampled here so a press that begins and ends inside a
	// long redraw is still seen.
	if (++pad_div >= PAD_DIV) {
		pad_div = 0;
		LP_PadSample();
	}
}

void LP_ClockInit(void)
{
	// Hold the channel stopped while it is reprogrammed.
	ITU_TSTR &= (uint8_t) ~ITU1_START;

	ITU_TCR1 = TCR_CLEAR_ON_GRA | TCR_CLOCK_DIV8;
	ITU_GRA1 = (uint16_t) (TICK_COUNTS - 1);

	// TCNT is deliberately not written: the channel clears itself on the
	// first compare match, and LoopyMSE asserts on any 16-bit TCNT access.

	(void) ITU_TSR1;
	ITU_TSR1 = (uint8_t) ~TSR_IMFA;
	ITU_TIER1 = TIER_IMIEA;

	clock_ticks = 0;

	// Priority 15, and unmask everything below it, matching what the
	// homebrew template does for the BIOS sound interrupt.
	sys_setInterruptPriority(INT_PRIO_ITU1, 0xF);
	sys_setInterruptMask(0xE);

	ITU_TSTR |= ITU1_START;
}

void LP_ClockRearm(void)
{
	uint32_t sr = LP_IrqBlock();

	ITU_TSTR &= (uint8_t) ~ITU1_START;
	ITU_TCR1 = TCR_CLEAR_ON_GRA | TCR_CLOCK_DIV8;
	ITU_GRA1 = (uint16_t) (TICK_COUNTS - 1);
	(void) ITU_TSR1;
	ITU_TSR1 = (uint8_t) ~TSR_IMFA;
	ITU_TIER1 = TIER_IMIEA;
	sys_setInterruptPriority(INT_PRIO_ITU1, 0xF);
	ITU_TSTR |= ITU1_START;

	LP_IrqRestore(sr);
}

uint32_t LP_ClockMS(void)
{
	return clock_ticks * TICK_MS;
}

uint32_t LP_ClockTicks(void)
{
	return clock_ticks;
}
