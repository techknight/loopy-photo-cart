/* lps.c -- initialisation, the tick, and the channel plan.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 * See LICENSE.
 */

#include "lps.h"
#include "lps_clock.h"
#include "lps_seq.h"
#include "lps_sfx.h"
#include "lps_voice.h"

/* Channel assignments per plan, as bitmasks over MIDI channels 0-3.
 *
 * See lps.h for why these are named rather than left to the caller. The short
 * version: channel 0 has no volume slider, so where music lives determines
 * whether a music volume control is possible at all.
 */
static const uint8_t plan_music[] = {
	0x09, /* MUSIC_LOUD:    channels 0 and 3 */
	0x06, /* MUSIC_MIXABLE: channels 1 and 2 (slider group 0) */
	0x00, /* SFX_ONLY */
	0x00 /* CUSTOM: taken from the config */
};

static const uint8_t plan_sfx[] = {
	0x06, /* MUSIC_LOUD:    channels 1 and 2 */
	0x09, /* MUSIC_MIXABLE: channels 0 and 3 */
	0x0F, /* SFX_ONLY:      everything */
	0x00 /* CUSTOM */
};

static uint8_t inited;
static uint8_t mode;
static uint8_t music_mask;
static uint8_t sfx_mask;
static uint8_t own_clock;
static volatile uint32_t millis;

int LPS_Init(const lps_config_t *cfg)
{
	if (inited)
		return LPS_ERR_ALREADY;
	if (!cfg || cfg->mode > LPS_MODE_1CH || cfg->plan > LPS_PLAN_CUSTOM)
		return LPS_ERR_CONFIG;

	if (cfg->plan == LPS_PLAN_CUSTOM) {
		if (cfg->music_chmask & cfg->sfx_chmask)
			return LPS_ERR_CONFIG;
		if ((cfg->music_chmask | cfg->sfx_chmask) & ~0x0F)
			return LPS_ERR_CONFIG;
		music_mask = cfg->music_chmask;
		sfx_mask = cfg->sfx_chmask;
	} else {
		music_mask = plan_music[cfg->plan];
		sfx_mask = plan_sfx[cfg->plan];
	}

	/* In the three-channel modes the synth ignores channel 3 entirely, and in
	 * single-channel mode only channel 0 exists. Silently handing a caller
	 * channels that will never sound is worse than correcting the mask. */
	if (cfg->mode == LPS_MODE_1CH) {
		music_mask &= 0x01;
		sfx_mask &= 0x01;
	} else if (cfg->mode == LPS_MODE_3CH || cfg->mode == LPS_MODE_3CH_RHYTHM) {
		music_mask &= 0x07;
		sfx_mask &= 0x07;
	}

	mode = cfg->mode;
	millis = 0;

	/* The order here is not negotiable.
	 *
	 * bios_soundChannels is what presses the synth's mode button; until it has
	 * run, every MIDI byte we send is discarded. The volume calls follow while
	 * the channels are still quiet. bios_initSoundTransmission does the
	 * handshake and configures SCI1 -- and then we re-assert the port
	 * ourselves, because from that point on it is ours and we want to know
	 * exactly what state it is in.
	 */
	LPS_PORT_BIOS_CHANNELS(mode);
	LPS_PORT_BIOS_VOLUME(LPS_VOL_GROUP0, cfg->vol_group0);
	LPS_PORT_BIOS_VOLUME(LPS_VOL_GROUP1, cfg->vol_group1);
	LPS_PORT_BIOS_INITTX();
	LPS_PORT_SCI_INIT();

	LPS_MidiInit();
	LPS_VoiceInit(music_mask, sfx_mask, mode);

	/* Last, because the clock's interrupt calls LPS_Tick and everything it
	 * touches must already be initialised. */
	own_clock = cfg->use_internal_clock ? 1 : 0;
	inited = 1;
	if (own_clock)
		LPS_ClockInit();

	return LPS_OK;
}

void LPS_Shutdown(void)
{
	if (!inited)
		return;

	/* Silence first, then stop the clock -- the other order would leave the
	 * panic bytes sitting in a queue that nothing is draining, and the synth
	 * holding notes forever. Draining takes a few ticks. */
	LPS_MusicStop();
	LPS_SfxSilence();
	LPS_PanicAll();
	if (own_clock)
		LPS_ClockDrainAndStop();

	inited = 0;
	own_clock = 0;
}

void LPS_Tick(uint16_t elapsed_ms)
{
	if (!inited)
		return;

	millis += elapsed_ms;

	/* Order matters. Everything that can queue bytes runs before the
	 * transmitter, so a note decided on this tick leaves on this tick rather
	 * than waiting for the next one.
	 *
	 * Claims expire before the sequencer runs, so a channel whose effect
	 * ended this tick is free for whatever the music wants to do with it. */
	LPS_VoiceTick(elapsed_ms);
	LPS_SeqTick(elapsed_ms);
	LPS_SfxTick(elapsed_ms);

	LPS_MidiTxTick();
}

uint32_t LPS_Millis(void)
{
	return millis;
}

void LPS_PanicAll(void)
{
	LPS_MidiPanic();
}

uint8_t LPS_MusicChannels(void)
{
	return music_mask;
}

uint8_t LPS_SfxChannels(void)
{
	return sfx_mask;
}

uint8_t LPS_Mode(void)
{
	return mode;
}
