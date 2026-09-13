/* lps_port.h -- the only file in lps/ that knows what machine it is running on.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 * See COPYING.md.
 *
 * The SCI1 register setup follows Kasami's loopy-homebrew-template (zlib; see
 * NOTICE.md).
 *
 *
 * Why macros and not a function table
 * ----------------------------------
 * With LPS_HOST set, a desktop program can run the REAL driver -- the same ring
 * queue, the same running-status logic, the same drop policy -- and produce
 * byte-for-byte the wire output the console would. That only works if there is
 * exactly one implementation. A link-time backend swap would mean the desktop
 * tests a stand-in, and every invariant would be checking the wrong code.
 *
 * So the port surface is function-like macros, and the rule that keeps #ifdef out
 * of every other file is: no logic module ever names a register. If you find
 * yourself wanting to write to SCI_TDR1 outside this header, add a macro instead.
 *
 * lps_clock.c is the single permitted exception -- its whole body is compiled out
 * on the desktop, because there is no desktop equivalent of a hardware timer and
 * pretending otherwise would be testing nothing.
 *
 *
 * Porting to a game that does not use loopy-homebrew-template
 * ----------------------------------------------------------
 * Define LPS_PORT_NO_TEMPLATE and supply the five LPS_PORT_* macros yourself.
 * That is the entire porting story.
 */

#ifndef LPS_PORT_H
#define LPS_PORT_H

#include <stdint.h>

#ifndef LPS_HOST
#define LPS_HOST 0
#endif

/* Interrupt-masking hooks.
 *
 * These are deliberately empty. The byte queue is single-producer /
 * single-consumer with volatile uint8_t indices: the main line only ever moves
 * the head, the tick only ever moves the tail, and a byte store is atomic with
 * respect to an interrupt taken between instructions. That has held up on
 * hardware.
 *
 * They exist as macros anyway so that a game running LPS_Tick from a different
 * priority level than its sound calls -- or on a future multi-core target -- has
 * somewhere to put the fix without editing the logic.
 */
#ifndef LPS_CRIT_ENTER
#define LPS_CRIT_ENTER() ((void) 0)
#endif
#ifndef LPS_CRIT_EXIT
#define LPS_CRIT_EXIT() ((void) 0)
#endif

/* Volume group and level selectors, mirroring SOUND_VOL_*. Named here so that a
 * caller never has to include a platform header to configure the library.
 *
 * Group 0 is MIDI channels 1 and 2; group 1 is channel 3. Channel 0 is not
 * attenuated by either.
 */
#define LPS_VOL_GROUP0 0
#define LPS_VOL_GROUP1 1
#define LPS_VOL_60 0
#define LPS_VOL_80 1
#define LPS_VOL_100 2

/* -------------------------------------------------------------------------- */

#if LPS_HOST

/* Desktop backend. The desktop program supplies these two functions, typically
 * timestamping every byte against its own virtual clock and logging it. */

#define LPS_HB_SCI_INIT 0
#define LPS_HB_CHANNELS 1
#define LPS_HB_VOLUME 2
#define LPS_HB_INITTX 3

void lps_host_tx(uint8_t b);
void lps_host_bios(int what, int a, int b);

#define LPS_ISR
#define LPS_PORT_TX(b) lps_host_tx((uint8_t) (b))
#define LPS_PORT_SCI_INIT() lps_host_bios(LPS_HB_SCI_INIT, 0, 0)
#define LPS_PORT_BIOS_CHANNELS(m) lps_host_bios(LPS_HB_CHANNELS, (m), 0)
#define LPS_PORT_BIOS_VOLUME(g, v) lps_host_bios(LPS_HB_VOLUME, (g), (v))
#define LPS_PORT_BIOS_INITTX() lps_host_bios(LPS_HB_INITTX, 0, 0)

#else /* Casio Loopy, SH-1 */

#ifndef LPS_PORT_NO_TEMPLATE
#include "loopy.h"
#include "sh7021.h"
#endif

/* -mrenesas is required for this attribute to generate a correct prologue. */
#define LPS_ISR __attribute__((interrupt_handler))

/* 16 MHz / (32 * (15 + 1)) = 31250 baud, the MIDI rate. */
#define LPS_SCI1_BRR 15

/* TE set, both serial interrupts off. SCI TXI/TEI are not usable here: LoopyMSE
 * does not implement them, so transmission is paced off the timer instead. */
#define LPS_SCI1_SCR 0x20

/* Re-assert the port after the BIOS has finished with it. The BIOS call is what
 * powers the synth on and muxes SCI1; from here the port is ours.
 *
 * The read of SSR1 before the write is not optional on real hardware -- a status
 * flag has to be read as 1 before it can be written back to 0. Under LoopyMSE the
 * read returns 0 and is logged, which is harmless once but is exactly why nothing
 * in this library ever polls a status bit in a loop.
 */
#ifndef LPS_PORT_SCI_INIT
#define LPS_PORT_SCI_INIT()                                                    \
	do {                                                                   \
		SCI_SMR1 = 0x00;                                               \
		SCI_BRR1 = LPS_SCI1_BRR;                                       \
		SCI_SCR1 = LPS_SCI1_SCR;                                       \
		(void) SCI_SSR1;                                               \
		SCI_SSR1 = 0x80;                                               \
	} while (0)
#endif

/* One byte onto the wire. Clearing TDRE (bit 7) is what starts the shift.
 *
 * Deliberately not a loop and deliberately not preceded by a TDRE poll. A byte
 * takes 320 us to shift out and the tick is at least 1 ms, so the previous byte
 * has always gone. Polling would hang under emulation (every SCI read returns 0,
 * so TDRE never appears set) and has no business in an interrupt handler anyway.
 */
#ifndef LPS_PORT_TX
#define LPS_PORT_TX(b)                                                         \
	do {                                                                   \
		SCI_TDR1 = (uint8_t) (b);                                      \
		SCI_SSR1 &= 0x7F;                                              \
	} while (0)
#endif

#ifndef LPS_PORT_BIOS_CHANNELS
#define LPS_PORT_BIOS_CHANNELS(m) bios_soundChannels(m)
#endif
#ifndef LPS_PORT_BIOS_VOLUME
#define LPS_PORT_BIOS_VOLUME(g, v) bios_soundVolume((g), (v))
#endif
#ifndef LPS_PORT_BIOS_INITTX
#define LPS_PORT_BIOS_INITTX() bios_initSoundTransmission()
#endif

#endif /* LPS_HOST */

#endif /* LPS_PORT_H */
