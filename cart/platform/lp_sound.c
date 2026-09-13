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
// (use_internal_clock = 0), holds the two effects, and runs the playlist.
//
// Channel plan: music on console channels 0, 1 and 3; effects alone on
// channel 2, so an effect never cuts a music note. An effect starts with a
// program change, which clears the channel, and a new effect replaces the
// one sounding (equal priority wins).
//

#include "lp_clock.h"
#include "lp_sound.h"

#include "lps.h"

// The background music: a playlist of public-domain pieces (see
// assets/music/SOURCES.md). By default La Candeur, then Clementi's Sonatina
// Op. 36 No. 1, then round again; both are baked without looping so the
// playlist can move on. A single song can be auditioned instead with
// EXTRA_CFLAGS=-DLPC_MUSIC_PASTORALE, _CLEMENTI or _GYMNOPEDIE.
#if defined(LPC_MUSIC_PASTORALE)
#include "music_pastorale.h"
static const lps_song_t *const playlist[] = { &lps_song_pastorale };
#elif defined(LPC_MUSIC_CLEMENTI)
#include "music_clementi.h"
static const lps_song_t *const playlist[] = { &lps_song_clementi };
#elif defined(LPC_MUSIC_GYMNOPEDIE)
#include "music_gymnopedie.h"
static const lps_song_t *const playlist[] = { &lps_song_gymnopedie };
#else
#include "music_candeur.h"
#include "music_clementi.h"
static const lps_song_t *const playlist[] = {
	&lps_song_candeur,
	&lps_song_clementi,
};
#endif

#define PLAYLIST_SONGS (sizeof playlist / sizeof playlist[0])

// A breath between one song ending and the next starting, in frames.
#define SONG_GAP_FRAMES 45

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
static uint8_t music_on;   // C toggles it
static uint8_t suspended;  // around a print
static uint8_t song;       // index into playlist
static uint8_t gap_frames;

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
	song = 0;
	LPS_MusicPlay(playlist[song]);
	music_on = 1;
}

// Called from LP_ClockISR every 2 ms tick. The driver's tick steps the
// sequencer and moves at most one MIDI byte.
void LP_SoundTick(void)
{
	if (sound_up)
		LPS_Tick(LP_CLOCK_TICK_MS);
}

// Once a frame, from the main line (LP_VideoPresent): starting a song is not
// something to do from the tick interrupt.
void LP_SoundFrame(void)
{
	// LPS_MusicPlaying() is also false while paused, so a paused or
	// suspended song must not count as finished.
	if (!sound_up || !music_on || suspended || LPS_MusicPlaying())
		return;
	if (++gap_frames < SONG_GAP_FRAMES)
		return;
	gap_frames = 0;
	song = (uint8_t) ((song + 1u) % PLAYLIST_SONGS);
	LPS_MusicPlay(playlist[song]);
}

void LP_SoundSuspend(void)
{
	if (!sound_up)
		return;
	suspended = 1;
	// Pausing releases the song's sounding notes; the tick keeps running
	// through a print, so the releases get out while the BIOS works.
	LPS_MusicPause(1);
	LPS_SfxSilence();
}

void LP_SoundResume(void)
{
	if (!sound_up)
		return;
	if (music_on)
		LPS_MusicPause(0);
	suspended = 0;
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
