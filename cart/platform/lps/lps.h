/* lps.h -- loopy-soundlib public interface.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 * See COPYING.md.
 *
 *
 * Sound for Casio Loopy homebrew. Include this and nothing else.
 *
 *     lps_config_t cfg = LPS_CONFIG_DEFAULT;
 *     LPS_Init(&cfg);
 *     ...
 *     LPS_Tick(2);          // from a 500 Hz timer, if you have your own
 *
 * The console's synth is reached only by clocking MIDI bytes out SCI1, and it is
 * a strange MIDI device: four channels, binary velocity, one working controller,
 * and a program change that silences whatever the channel was playing. This
 * library's job is to make those constraints someone else's problem.
 */

#ifndef LPS_H
#define LPS_H

#include <stdint.h>

#include "lps_port.h"
#include "lps_inst.h"
#include "lps_midi.h"
#include "lps_voice.h"
#include "lps_seq.h"
#include "lps_sfx.h"

#define LPS_VERSION_MAJOR 0
#define LPS_VERSION_MINOR 1

/* Ticks per second for the internal clock, and the rate an externally-driven
 * LPS_Tick is assumed to run at.
 *
 * 500 Hz is proven on hardware. One byte leaves per tick, so this is also the
 * wire budget: 500 bytes/second out of a possible 3125.
 *
 * 1000 Hz doubles that. Under emulation it measured exactly 1000 bytes/s,
 * byte-perfect across a 24,000-byte saturation run, no emulator assertions. It is
 * a supported value -- but the emulator models a byte as 8 bit-times where
 * hardware sends 10, so the real margin is 3.1x rather than the 3.9x observed.
 * That is still comfortable, but it is unverified on a real console, so it is
 * opt-in.
 *
 * 2 bytes per tick is untested and should not be enabled: 64% wire occupancy
 * with no jitter margin, on an emulator that flatters it by 25%.
 *
 * There is a hard floor at 500: the transmitter can hold at most two bytes and
 * cannot be polled, so a slower tick simply cannot move enough data to be useful.
 */
#ifndef LPS_TICK_HZ
#define LPS_TICK_HZ 500
#endif
#if LPS_TICK_HZ < 500
#error "LPS_TICK_HZ below 500 cannot sustain a usable byte rate"
#endif

/* Channel modes, matching SOUND_CHANS_*. Polyphony per channel is 6/4/2/4 in
 * 4-channel mode and 12 on channel 0 alone in 1-channel mode -- halved for the
 * twelve layered presets. Use LPS_PolyCap() rather than remembering that. */
typedef enum {
	LPS_MODE_4CH = 0,
	LPS_MODE_3CH_RHYTHM = 1,
	LPS_MODE_3CH = 2,
	LPS_MODE_1CH = 3
} lps_mode_t;

/* Who gets which channels.
 *
 * This is a first-class choice rather than something left to the caller, because
 * getting it wrong is invisible until it is too late: the hardware volume sliders
 * do not touch channel 0 at all. A game that puts music on channel 0 and then
 * ships a music volume slider has built something that cannot work.
 *
 * Channel 2 is worth a note of its own. It has two voices -- one, on a layered
 * preset -- and Casio's own games barely use it (0.1% of events across eight
 * retail titles sampled). It is a poor place for a melody
 * and a good place for a single effect that must never be interrupted.
 */
typedef enum {
	/* Music on 0 and 3, effects on 1 and 2. Music is unattenuable and gets
	 * the biggest voice pool. Pick this unless the game has a music slider. */
	LPS_PLAN_MUSIC_LOUD = 0,

	/* Music on 1 and 2 (slider group 0), effects on 0 and 3. Music is
	 * mixable; effects on channel 0 always cut through. Costs music the
	 * 6-voice channel. */
	LPS_PLAN_MUSIC_MIXABLE = 1,

	/* No music. All four channels available to effects. */
	LPS_PLAN_SFX_ONLY = 2,

	/* Use music_chmask and sfx_chmask verbatim. They must not overlap. */
	LPS_PLAN_CUSTOM = 3
} lps_plan_t;

typedef struct {
	/* Set once. There is no setter, and that is deliberate: the synth's mode
	 * state machine can reach 4-channel from 3-channel but not the reverse
	 * without a full re-init, and every transition silences all four channels
	 * and invalidates every cached program. A runtime mode change is a bug
	 * waiting to happen, so the API does not offer one. */
	uint8_t mode; /* lps_mode_t */

	uint8_t plan; /* lps_plan_t */
	uint8_t music_chmask; /* LPS_PLAN_CUSTOM only */
	uint8_t sfx_chmask; /* LPS_PLAN_CUSTOM only */

	/* 1: the library runs ITU1 itself and calls LPS_Tick internally. The game
	 * must route LPS_ClockISR into its vector table.
	 *
	 * 0: the game already has a timer and calls LPS_Tick from it. This is a
	 * first-class path, not a fallback -- most games have a tick already and
	 * a second one costs interrupts for nothing. The rate must be at least
	 * 500 Hz. */
	uint8_t use_internal_clock;

	uint8_t vol_group0; /* LPS_VOL_60/80/100 -- MIDI channels 1 and 2 */
	uint8_t vol_group1; /* LPS_VOL_60/80/100 -- MIDI channel 3 */
} lps_config_t;

#define LPS_CONFIG_DEFAULT                                                     \
	{                                                                      \
		LPS_MODE_4CH, LPS_PLAN_MUSIC_LOUD, 0, 0, 1, LPS_VOL_100,       \
			LPS_VOL_100                                            \
	}

#define LPS_OK 0
#define LPS_ERR_CONFIG (-1)
#define LPS_ERR_ALREADY (-2)

/* Bring up the synth and the driver. Safe to call once; returns LPS_ERR_ALREADY
 * on a second call without an intervening LPS_Shutdown. */
int LPS_Init(const lps_config_t *cfg);

/* Silence everything and stop the internal clock if we started it. */
void LPS_Shutdown(void);

/* Advance the library by elapsed_ms and move one byte to the wire.
 *
 * Interrupt-safe. Call at LPS_TICK_HZ. elapsed_ms is the tick period -- it is a
 * parameter rather than a constant so that a game with an odd timer rate can be
 * honest about it, but exactly one byte is transmitted per call regardless, so a
 * slow tick is a slow wire.
 */
void LPS_Tick(uint16_t elapsed_ms);

/* Milliseconds since LPS_Init, as counted by the ticks. */
uint32_t LPS_Millis(void);

/* Stop everything, immediately and unconditionally. Eight bytes, jumps the drop
 * policy. This is the "something has gone wrong" button; nothing in normal
 * operation needs it. */
void LPS_PanicAll(void);

/* The channel masks in force, derived from the plan. */
uint8_t LPS_MusicChannels(void);
uint8_t LPS_SfxChannels(void);

/* The configured channel mode, for LPS_PolyCap(). */
uint8_t LPS_Mode(void);

#endif /* LPS_H */
