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
// Sound backend: thin glue over the sound driver in platform/lps, set up the
// way LoopyPuzzleBobble runs it on hardware.
//
// The driver owns the synth (channel plan, MIDI queue, sequencer, effects);
// this file brings it up, feeds it time from our ITU1 tick
// (use_internal_clock = 0) and holds the two effects.
//
// Channel plan: music on console channels 0, 1 and 3; effects alone on
// channel 2, so an effect never cuts a music note. An effect starts with a
// program change, which clears the channel, and a new effect replaces the
// one sounding (equal priority wins).
//

#include "lp_clock.h"
#include "lp_sound.h"

#include "lps.h"

#include "music_gymnopedie.h"

#define MUSIC_CHANNELS  0x0B
#define EFFECT_CHANNELS 0x04

// Program 10: its high notes are short clicks, the timbre retail Loopy games
// use for menu sounds (loopy-soundlib docs/retail-sfx.md).
#define PROGRAM_CLICK 10

static const lps_sfxnote_t sfx_notes[] = {
	// LP_SFX_MOVE: note 96, the menu-cursor click of four retail games.
	{ 0, LPS_SFXNOTE_MAKE(LPS_SFXOP_NOTE, 0), 96, 0 },
	// LP_SFX_BUTTON: a short rising chirp, two clicks 40 ms apart.
	{ 0, LPS_SFXNOTE_MAKE(LPS_SFXOP_NOTE, 0), 84, 0 },
	{ 5, LPS_SFXNOTE_MAKE(LPS_SFXOP_NOTE, 0), 91, 0 },
};

static const lps_gesture_t sfx_gestures[] = {
	// first, count, priority, channels wanted (one), progset, flags, length
	[LP_SFX_MOVE]   = { 0, 1, LPS_PRI_UI, 0x01, 0, LPS_SFX_F_CLEAR, 4 },
	[LP_SFX_BUTTON] = { 1, 2, LPS_PRI_UI, 0x01, 0, LPS_SFX_F_CLEAR, 12 },
};

// Programs per gesture slot: slot 0 is the gesture's first channel.
static const uint8_t sfx_progsets[] = { PROGRAM_CLICK, 0xFF, 0xFF, 0xFF };

static const lps_sfxbank_t sfx_bank = {
	sfx_notes, sizeof sfx_notes / sizeof sfx_notes[0],
	sfx_gestures, sizeof sfx_gestures / sizeof sfx_gestures[0],
	sfx_progsets, 1, 0,
};

static uint8_t sound_up;
static uint8_t music_on;

void LP_SoundInit(void)
{
	lps_config_t cfg = LPS_CONFIG_DEFAULT;

	cfg.plan = LPS_PLAN_CUSTOM;
	cfg.music_chmask = MUSIC_CHANNELS;
	cfg.sfx_chmask = EFFECT_CHANNELS;
	cfg.use_internal_clock = 0;

	if (LPS_Init(&cfg) != LPS_OK)
		return;
	sound_up = 1;

	LPS_SfxSetBank(&sfx_bank);
	LPS_MusicPlay(&lps_song_gymnopedie);
	music_on = 1;
}

// Called from LP_ClockISR every 2 ms tick. The driver's tick steps the
// sequencer and moves at most one MIDI byte.
void LP_SoundTick(void)
{
	if (sound_up)
		LPS_Tick(LP_CLOCK_TICK_MS);
}

void LP_SoundSuspend(void)
{
	if (!sound_up)
		return;
	// Pausing releases the song's sounding notes; the tick keeps running
	// through a print, so the releases get out while the BIOS works.
	LPS_MusicPause(1);
	LPS_SfxSilence();
}

void LP_SoundResume(void)
{
	if (sound_up && music_on)
		LPS_MusicPause(0);
}

void LP_SfxPlay(unsigned id)
{
	if (sound_up)
		(void) LPS_SfxPlay((uint16_t) id);
}

void LP_MusicToggle(void)
{
	if (!sound_up)
		return;
	music_on = !music_on;
	LPS_MusicPause(!music_on);
}
