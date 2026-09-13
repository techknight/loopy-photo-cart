/* lps_clock.c -- ITU1 compare-match A at LPS_TICK_HZ.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 *
 * This is the one file in lps/ that compiles to nothing on the host. There is no
 * desktop equivalent of a hardware timer, and a simulated one would only be
 * testing itself -- the harness drives LPS_Tick directly instead, which is the
 * same thing this interrupt does.
 *
 *
 * What LoopyMSE cannot do, which shapes every line below
 * -----------------------------------------------------
 * The emulator's timer model is narrow, and the shipped build has asserts
 * compiled out, so violating it produces wrong behaviour rather than a message.
 * From reading LoopyMSE's src/core/sh2/peripherals/sh2_timers.cpp:
 *
 *   - Every 16-bit ITU read asserts and returns 0. Every 16-bit write asserts
 *     except to GRA and GRB. An 8-bit TCNT write asserts unless the value is 0.
 *     So TCNT is never touched here, at all, in either direction.
 *   - Compare-match B and counter overflow are not implemented and assert. Only
 *     compare-match A is usable.
 *   - Of the 8-bit reads only TSR works, which is enough: that is the one this
 *     needs.
 *   - The prescaler must be an internal clock (TPSC 0-3), TIOR must be zero, and
 *     CCLR must not be 11.
 *
 * And from the interrupt controller: only IPRC is implemented, which holds ITU0
 * and ITU1. ITU0 is the BIOS's. Hence channel 1, and no alternative.
 */

#include "lps_clock.h"

#if !LPS_HOST

#include "lps.h"

#ifndef LPS_PORT_NO_TEMPLATE
#include "loopy.h"
#include "loopy/extrafuncs.h"
#include "sh7021.h"
#endif

/* phi/8 = 2 MHz. */
#define CLOCK_HZ (F_CPU / 8)
#define TICK_COUNTS (CLOCK_HZ / LPS_TICK_HZ)
#define TICK_MS (1000 / LPS_TICK_HZ)

#if TICK_COUNTS > 65535
#error "LPS_TICK_HZ too low: the compare value does not fit GRA1"
#endif
#if (CLOCK_HZ % LPS_TICK_HZ) != 0
#warning "LPS_TICK_HZ does not divide 2 MHz exactly; the tick will drift slightly"
#endif

#define ITU1_START 0x02 /* TSTR bit 1 */
#define TCR_CLEAR_ON_GRA 0x20 /* CCLR = 01 */
#define TCR_CLOCK_DIV8 0x03 /* TPSC = 11 */
#define TIER_IMIEA 0x01
#define TSR_IMFA 0x01

/* At 500 Hz a full 64-byte queue drains in 128 ms. Ten times that is a generous
 * ceiling that still cannot hang a shutdown if the transmitter is wedged. */
#define DRAIN_LIMIT_TICKS (LPS_QUEUE_SIZE * 10)

static uint8_t running;

void LPS_ClockInit(void)
{
	if (running)
		return;

	/* Hold the channel stopped while it is reprogrammed. */
	ITU_TSTR &= (uint8_t) ~ITU1_START;

	ITU_TCR1 = TCR_CLEAR_ON_GRA | TCR_CLOCK_DIV8;
	ITU_GRA1 = (uint16_t) (TICK_COUNTS - 1);

	/* TCNT is deliberately not written -- see the header comment. The channel
	 * is stopped and clears itself on the first compare match, so at worst the
	 * very first tick is short, which nothing can hear. */

	/* A status flag must be read as 1 before it can be written back as 0. */
	(void) ITU_TSR1;
	ITU_TSR1 = (uint8_t) ~TSR_IMFA;
	ITU_TIER1 = TIER_IMIEA;

	/* Priority 15 and unmask everything below it, matching what the homebrew
	 * template does for the BIOS sound interrupt. Sound is the one thing that
	 * cannot be late: a dropped video frame is a stutter, a dropped tick is a
	 * byte that never reaches the synth. */
	sys_setInterruptPriority(INT_PRIO_ITU1, 0xF);
	sys_setInterruptMask(0xE);

	running = 1;
	ITU_TSTR |= ITU1_START;
}

void LPS_ClockStop(void)
{
	ITU_TSTR &= (uint8_t) ~ITU1_START;
	ITU_TIER1 = 0;
	running = 0;
}

void LPS_ClockDrainAndStop(void)
{
	int guard = DRAIN_LIMIT_TICKS;

	/* Spin on the queue, not on a status register. LPS_MidiFree is updated by
	 * the interrupt that is still running; the guard is there because a
	 * transmitter that has stopped moving would otherwise hang shutdown. */
	while (guard-- > 0 && LPS_MidiFree() < LPS_QUEUE_SIZE - 1)
		;

	LPS_ClockStop();
}

LPS_ISR void LPS_ClockISR(void)
{
	/* Read-then-write-zero, as above. */
	(void) ITU_TSR1;
	ITU_TSR1 = (uint8_t) ~TSR_IMFA;

	LPS_Tick(TICK_MS);
}

#else /* LPS_HOST */

/* On the host the harness IS the clock -- it advances a counter and calls
 * LPS_Tick itself, which is exactly what the interrupt above does.
 *
 * These stubs exist so that lps.c can call the clock unconditionally and stay
 * free of preprocessor conditionals. They are never reached: the harness forces
 * use_internal_clock to 0. Should that change, doing nothing is still correct --
 * the harness would keep ticking regardless.
 */

void LPS_ClockInit(void)
{
}

void LPS_ClockStop(void)
{
}

void LPS_ClockDrainAndStop(void)
{
}

#endif /* !LPS_HOST */
