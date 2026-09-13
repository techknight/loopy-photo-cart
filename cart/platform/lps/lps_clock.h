/* lps_clock.h -- optional self-contained ITU1 tick.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 *
 * Only needed if lps_config_t.use_internal_clock is 1. A game that already has a
 * timer should call LPS_Tick from it instead and never include this -- one
 * interrupt is cheaper than two, and the library does not need to own a channel.
 *
 * If you do use it, put LPS_ClockISR in your vector table at the ITU1 IMIA slot:
 *
 *     // ITU0..ITU4 (IMIA, IMIB, OVI, Reserved)
 *     bios_isrAutoUpdateBgm, doNothing, doNothing, RESERVED,
 *     LPS_ClockISR,          doNothing, doNothing, RESERVED,
 *
 * ITU1 and not another channel, for two independent reasons: ITU0 belongs to the
 * BIOS (its BGM sequencer sits on that vector), and ITU2-4 have their priorities
 * in IPRD, which LoopyMSE does not implement -- so a higher channel cannot even
 * be configured under emulation. Channel 1 works on both.
 */

#ifndef LPS_CLOCK_H
#define LPS_CLOCK_H

#include <stdint.h>

#include "lps_port.h"

/* Start ITU1 at LPS_TICK_HZ. Safe to call more than once. */
void LPS_ClockInit(void);

/* Stop ITU1 immediately. Anything still queued is abandoned. */
void LPS_ClockStop(void);

/* Keep ticking until the byte queue empties, then stop. This is what shutdown
 * wants: the panic bytes have to reach the wire before the thing that carries
 * them to the wire goes away. Bounded, so a wedged transmitter cannot hang. */
void LPS_ClockDrainAndStop(void);

/* Compare-match A handler. Reference this from the vector table; do not call it. */
LPS_ISR void LPS_ClockISR(void);

#endif /* LPS_CLOCK_H */
