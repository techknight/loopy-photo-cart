/* lps_voice.c -- effects-channel arbitration.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 */

#include "lps_voice.h"

typedef struct {
	uint8_t prio;
	uint16_t remain_ms; /* 0 = free */
} chan_t;

static chan_t chan[LPS_CHANNELS];
static uint8_t music_mask;
static uint8_t sfx_mask;
static uint16_t refused;

void LPS_VoiceInit(uint8_t music, uint8_t sfx, uint8_t mode)
{
	uint8_t ch;

	(void) mode;
	music_mask = music;
	sfx_mask = sfx;
	refused = 0;
	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		chan[ch].prio = 0;
		chan[ch].remain_ms = 0;
	}
}

uint8_t LPS_VoiceMusicMask(void)
{
	return music_mask;
}

uint8_t LPS_VoiceSfxMask(void)
{
	return sfx_mask;
}

uint16_t LPS_VoiceRefused(void)
{
	return refused;
}

int LPS_VoiceBusy(uint8_t ch)
{
	return ch < LPS_CHANNELS && chan[ch].remain_ms != 0;
}

uint8_t LPS_VoicePriority(uint8_t ch)
{
	if (ch >= LPS_CHANNELS || !chan[ch].remain_ms)
		return 0;
	return chan[ch].prio;
}

int LPS_VoiceClaim(uint8_t chmask, uint8_t prio, uint16_t hold_ms)
{
	uint8_t ch;

	chmask &= 0x0F;
	if (!chmask)
		return 0;

	/* Anything outside the effects channels is not on offer. Music owns its
	 * channels for the whole session -- see the header for why. */
	if (chmask & ~sfx_mask) {
		refused++;
		return 0;
	}

	/* Check every channel before taking any.
	 *
	 * A gesture that gets two of the three channels it wanted does not sound
	 * like a quieter version of itself; it sounds like a different, broken
	 * sound. So the test runs to completion first and the mutation happens
	 * only if all of it passed.
	 */
	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (!(chmask & (1u << ch)))
			continue;
		/* Equal priority wins, so an effect can retrigger itself. Only a
		 * strictly higher priority refuses. */
		if (chan[ch].remain_ms && chan[ch].prio > prio) {
			refused++;
			return 0;
		}
	}

	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (!(chmask & (1u << ch)))
			continue;
		chan[ch].prio = prio;
		/* A claim never shortens an existing one: two effects sharing a
		 * channel at the same priority should leave it held until the
		 * later of the two finishes. */
		if (hold_ms > chan[ch].remain_ms)
			chan[ch].remain_ms = hold_ms;
	}
	return 1;
}

void LPS_VoiceRelease(uint8_t chmask)
{
	uint8_t ch;

	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (chmask & (1u << ch)) {
			chan[ch].prio = 0;
			chan[ch].remain_ms = 0;
		}
	}
}

void LPS_VoiceTick(uint16_t elapsed_ms)
{
	uint8_t ch;

	/* Counting down rather than comparing against a wall clock is deliberate:
	 * a countdown cannot be confused by the millisecond counter wrapping, and
	 * an effect that outlives a wrap would otherwise look like one that ended
	 * 65 seconds ago. */
	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (!chan[ch].remain_ms)
			continue;
		if (chan[ch].remain_ms <= elapsed_ms) {
			chan[ch].remain_ms = 0;
			chan[ch].prio = 0;
		} else {
			chan[ch].remain_ms = (uint16_t) (chan[ch].remain_ms -
							 elapsed_ms);
		}
	}
}
