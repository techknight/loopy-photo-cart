/* lps_seq.c -- the music sequencer.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 * Adapted in part from LoopyDOOM's platform/i_music_loopy.c (GPL-2.0-or-later).
 * See NOTICE.md.
 *
 *
 * Concurrency
 * -----------
 * `playing` is the gate. The main line clears it before touching anything the
 * tick reads, and sets it last; the tick does nothing at all while it is clear.
 * No masking, no lock. This is LoopyDOOM's discipline and it holds for the same
 * reason the byte queue's does -- a single volatile byte, written by one side.
 *
 * The one rule that follows from it: never mutate song state without clearing
 * the gate first, however small the change looks.
 */

#include "lps_seq.h"

#include "lps_midi.h"
#include "lps_voice.h"

/* Fade stages, as fractions of the fade duration.
 *
 * Thinning has to start with the notes that will be missed least. Dropping
 * note-ons on channels that already have something sounding removes inner voices
 * and doubled parts first, leaving the melody, which is what a listener tracks.
 * Only then does everything stop.
 */
#define FADE_STAGE1_PCT 40 /* thin: drop note-ons on busy channels */
#define FADE_STAGE2_PCT 80 /* drop all note-ons; releases still pass */

static const lps_song_t *song;
static volatile uint8_t playing;
static uint8_t paused;

static uint16_t idx;
static uint32_t pos_ms;
static uint32_t next_ms; /* absolute time of event `idx` */

static uint16_t fade_total;
static uint16_t fade_left;

static uint8_t marker;

static void silence_song_channels(void)
{
	uint8_t ch;

	if (!song)
		return;
	for (ch = 0; ch < LPS_CHANNELS; ch++)
		if (song->chmask & (1u << ch))
			LPS_SilenceChannel(ch);
}

static void rewind_to(uint16_t ev)
{
	idx = ev;
	next_ms = pos_ms + (ev < song->n_events ? song->evs[ev].dt : 0);
}

void LPS_MusicPlay(const lps_song_t *s)
{
	uint8_t ch;

	if (!s || !s->evs || !s->n_events)
		return;

	/* Gate down before anything else, so the tick cannot observe a song
	 * half-swapped. */
	playing = 0;
	silence_song_channels();

	song = s;
	paused = 0;
	pos_ms = 0;
	next_ms = s->evs[0].dt;
	idx = 0;
	fade_total = 0;
	fade_left = 0;
	marker = 0;

	for (ch = 0; ch < LPS_CHANNELS; ch++)
		if ((s->chmask & (1u << ch)) && s->prog[ch] != 0xFF)
			LPS_Program(ch, s->prog[ch]);

	playing = 1;
}

void LPS_MusicStop(void)
{
	playing = 0;
	silence_song_channels();
	song = 0;
	fade_left = 0;
}

void LPS_MusicPause(int p)
{
	if (!song)
		return;
	if (p && !paused) {
		paused = 1;
		/* Release what is sounding rather than freezing it. A held note
		 * on this synth does not decay -- pausing without releasing
		 * leaves a chord droning for as long as the menu is open. */
		silence_song_channels();
	} else if (!p && paused) {
		paused = 0;
	}
}

void LPS_MusicFadeOut(uint16_t ms)
{
	if (!song || !playing)
		return;
	if (!ms) {
		LPS_MusicStop();
		return;
	}
	fade_total = ms;
	fade_left = ms;
}

int LPS_MusicPlaying(void)
{
	return playing && !paused;
}

uint32_t LPS_MusicPosMs(void)
{
	return pos_ms;
}

uint8_t LPS_MusicTakeMarker(void)
{
	uint8_t m = marker;

	marker = 0;
	return m;
}

/* How far through the fade we are, 0..100. 0 when not fading. */
static uint8_t fade_pct(void)
{
	if (!fade_total || !fade_left)
		return 0;
	return (uint8_t) (100u - (100u * fade_left) / fade_total);
}

/* Should this note-on be dropped as part of a fade? */
static int fade_drops_note(uint8_t ch)
{
	uint8_t pct;

	if (!fade_total)
		return 0;
	pct = fade_pct();
	if (pct >= FADE_STAGE2_PCT)
		return 1;
	if (pct >= FADE_STAGE1_PCT)
		return LPS_PolyNow(ch) > 0;
	return 0;
}

static void do_event(const lps_ev_t *e)
{
	uint8_t op = LPS_EV_OP(e);
	uint8_t ch = LPS_EV_CH(e);
	uint8_t low = LPS_EV_LOW(e);

	/* A song only ever speaks on the channels it declared. Anything else is
	 * a baker bug, and letting it through would let a song stamp on an
	 * effect. */
	if (!(song->chmask & (1u << ch)))
		return;

	switch (op) {
	case LPS_OP_NOTE:
		if (low & 1) {
			if (!fade_drops_note(ch))
				LPS_NoteOn(ch, e->arg);
		} else {
			/* Note-offs always pass. A dropped release is a note
			 * that never stops -- far worse than a missing note,
			 * and during a fade it is the whole point. */
			LPS_NoteOff(ch, e->arg);
		}
		break;

	case LPS_OP_PROG:
		LPS_Program(ch, e->arg);
		break;

	case LPS_OP_CTRL:
		if (low == LPS_CTRL_BEND) {
			/* The baker stores the MSB the synth actually reads.
			 * Convert back to cents for the driver's API. */
			int16_t cents = (int16_t) (((int16_t) e->arg - 64) * 200 / 64);

			LPS_Bend(ch, cents);
		}
		break;

	case LPS_OP_META:
		switch (low) {
		case LPS_META_MARKER:
			marker = e->arg;
			break;
		case LPS_META_END:
			silence_song_channels();
			if ((song->flags & LPS_SONG_LOOPS) &&
			    song->loop_event < song->n_events) {
				uint8_t ch2;

				/* Re-assert programs on the way round: a song
				 * may have changed instrument mid-tune, and the
				 * loop has to start from the same state the
				 * first pass did. */
				for (ch2 = 0; ch2 < LPS_CHANNELS; ch2++)
					if ((song->chmask & (1u << ch2)) &&
					    song->prog[ch2] != 0xFF)
						LPS_Program(ch2, song->prog[ch2]);
				rewind_to(song->loop_event);
			} else {
				playing = 0;
			}
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

void LPS_SeqTick(uint16_t elapsed_ms)
{
	int budget = LPS_SEQ_MAX_EV_PER_TICK;

	if (!playing || paused || !song)
		return;

	pos_ms += elapsed_ms;

	if (fade_left) {
		if (fade_left <= elapsed_ms) {
			/* The fade has run out. Stop for real -- and note that
			 * LPS_MusicStop clears `song`, so nothing below may
			 * touch it afterwards. */
			LPS_MusicStop();
			return;
		}
		fade_left = (uint16_t) (fade_left - elapsed_ms);
	}

	while (playing && idx < song->n_events && pos_ms >= next_ms) {
		const lps_ev_t *e = &song->evs[idx];

		if (budget-- <= 0)
			break;

		do_event(e);

		/* do_event can stop the song or move the playhead, in which case
		 * idx and next_ms are already right and must not be advanced. */
		if (!playing)
			return;
		if (e != &song->evs[idx])
			continue;

		idx++;
		if (idx < song->n_events)
			next_ms += song->evs[idx].dt;
	}

	/* Ran off the end without an explicit end marker. Treat it as one, so a
	 * baker that forgot the terminator still behaves. */
	if (playing && idx >= song->n_events) {
		silence_song_channels();
		if ((song->flags & LPS_SONG_LOOPS) &&
		    song->loop_event < song->n_events)
			rewind_to(song->loop_event);
		else
			playing = 0;
	}
}
