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
// Free-running millisecond clock for the Casio Loopy build.
//

#ifndef __LP_CLOCK_H__
#define __LP_CLOCK_H__

#include <stdint.h>

// Start the hardware clock. Safe to call more than once.
void LP_ClockInit(void);

// Reprogram the timer after something outside the program (a BIOS print) may
// have stopped or changed it. The tick count carries on.
void LP_ClockRearm(void);

// Milliseconds since LP_ClockInit(). Counts real elapsed time whatever the
// CPU is doing -- see lp_clock.c.
uint32_t LP_ClockMS(void);

// Raw compare-match ticks since LP_ClockInit(), in LP_CLOCK_TICK_MS units.
uint32_t LP_ClockTicks(void);

// Duration of one tick.
#define LP_CLOCK_TICK_MS 2

// ITU1 compare-match handler. Referenced by the vector table in boot/.
void LP_ClockISR(void);

#endif
