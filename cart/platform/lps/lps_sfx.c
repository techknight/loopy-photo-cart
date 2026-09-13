/* lps_sfx.c -- the sound-effect engine.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 */

#include "lps_sfx.h"

#include "lps_midi.h"
#include "lps_voice.h"

typedef struct {
	uint16_t at_tick;  /* absolute 8 ms tick to fire on */
	uint8_t ch;
	uint8_t op;
	uint8_t arg;
	uint8_t gate8;
} pending_t;

static const lps_sfxbank_t *bank;
static pending_t pending[LPS_SFX_MAX_PENDING];
static uint8_t n_pending;

/* One release per channel, not per note. A channel holds one effect at a time,
 * and an effect that wants two notes released separately is really two effects. */
static struct {
	uint8_t note;
	uint8_t sounding;
	uint16_t release_tick;
} voice[LPS_CHANNELS];

static uint16_t now_tick;
static uint16_t accum_ms;
static uint8_t enabled = 1;
static uint16_t refused;

/* Map a gesture's channel index onto a real MIDI channel.
 *
 * A gesture says "my channel 0" and "my channel 1"; which hardware channels those
 * are depends on the plan the game chose at init. Baking real numbers into the
 * bank would tie the content to one layout, so the mask is resolved here.
 */
static uint8_t resolve_ch(uint8_t mask, uint8_t index)
{
	uint8_t ch;

	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (!(mask & (1u << ch)))
			continue;
		if (!index)
			return ch;
		index--;
	}
	return 0xFF;
}

/* The channels a gesture will actually use, from the effects channels available.
 *
 * A gesture wanting two channels gets the first two the plan set aside. If the
 * plan has fewer than the gesture needs, it cannot play. */
static uint8_t gesture_channels(const lps_gesture_t *g)
{
	uint8_t avail = LPS_VoiceSfxMask();
	uint8_t want = 0;
	uint8_t got = 0;
	uint8_t ch;
	uint8_t n = 0;

	for (ch = 0; ch < LPS_CHANNELS; ch++)
		if (g->chmask & (1u << ch))
			n++;

	for (ch = 0; ch < LPS_CHANNELS && n; ch++) {
		if (!(avail & (1u << ch)))
			continue;
		got |= (uint8_t) (1u << ch);
		n--;
	}
	if (n)
		return 0; /* the plan does not have enough effects channels */
	want = got;
	return want;
}

void LPS_SfxSetBank(const lps_sfxbank_t *b)
{
	bank = b;
	n_pending = 0;
}

void LPS_SfxEnable(int on)
{
	if (!on && enabled)
		LPS_SfxSilence();
	enabled = on ? 1 : 0;
}

uint16_t LPS_SfxRefused(void)
{
	return refused;
}

static void release_voice(uint8_t ch)
{
	if (!voice[ch].sounding)
		return;
	LPS_NoteOff(ch, voice[ch].note);
	voice[ch].sounding = 0;
}

void LPS_SfxSilence(void)
{
	uint8_t ch;
	uint8_t mask = LPS_VoiceSfxMask();

	n_pending = 0;
	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		if (!(mask & (1u << ch)))
			continue;
		/* A program change, for the same reason the pre-gesture clear is
		 * one: a gesture may have layered a second copy of a note, and no
		 * note-off can reach it. Only killing the channel's voices will. */
		LPS_Program(ch, 0x7F);
		voice[ch].sounding = 0;
	}
	LPS_VoiceRelease(mask);
}

int LPS_SfxPlay(uint16_t id)
{
	const lps_gesture_t *g;
	uint8_t chans;
	uint16_t i;
	uint16_t hold_ms;

	if (!bank || !enabled || id >= bank->n_gestures)
		return 0;

	g = &bank->gestures[id];
	if (!g->count || (uint32_t) g->first + g->count > bank->n_notes)
		return 0;

	chans = gesture_channels(g);
	if (!chans) {
		refused++;
		return 0;
	}

	/* Everything or nothing.
	 *
	 * A gesture that gets some of its notes queued and not the rest does not
	 * sound like a quieter version of itself -- it sounds like a different,
	 * broken effect. So both resources are checked before either is taken. */
	if (n_pending + g->count > LPS_SFX_MAX_PENDING) {
		refused++;
		return 0;
	}

	hold_ms = (uint16_t) (g->dur8 * LPS_SFX_TICK_MS + LPS_SFX_TICK_MS);
	if (!LPS_VoiceClaim(chans, g->priority, hold_ms))
		return 0; /* lps_voice counts this refusal itself */

	/* Clear each channel before use.
	 *
	 * This has to be a program change, not LPS_SilenceChannel, and the reason
	 * is the whole justification for the flag existing. A note-off names one
	 * note, and the synth's note-off stops at the FIRST voice matching that
	 * pitch -- so on a channel where LPS_SFXOP_LAYER struck a note twice, a
	 * release silences one copy and strands the other permanently.
	 *
	 * A program change kills every voice on the channel, layered copies
	 * included. Program 0x7F is above the patch range, so the synth runs its
	 * silencing loop and then rejects the program, leaving the instrument
	 * untouched: two bytes to reset the channel to nothing sounding.
	 *
	 * The retail games reach for exactly this, which is why they can double
	 * notes freely and never send a note-off at all (docs/retail-sfx.md).
	 */
	/* The clear and the timbre select are the same two bytes.
	 *
	 * Re-sending a program does both jobs at once, which is why the retail
	 * games spend two bytes an effect where the obvious implementation spends
	 * four. LPS_ProgramForce is the entry point that skips the redundancy
	 * suppression LPS_Program applies, because here the "redundant" silence is
	 * precisely what is wanted.
	 *
	 * Slots are indexed the way the notes are -- slot 0 is the gesture's first
	 * channel, not hardware channel 0. Indexing by hardware channel would tie a
	 * bank to one channel plan and would fail *silently*, because unset slots
	 * hold 0xFF and 0xFF means "leave this channel alone": the effect would
	 * play on whatever timbre happened to be there.
	 */
	{
		const uint8_t *ps = (bank->progsets && g->progset < bank->n_progsets)
			? &bank->progsets[g->progset * 4]
			: 0;
		uint8_t slot;

		for (slot = 0; slot < LPS_CHANNELS; slot++) {
			uint8_t ch = resolve_ch(chans, slot);
			uint8_t prog = ps ? ps[slot] : 0xFF;

			if (ch == 0xFF)
				break;
			if (prog != 0xFF)
				LPS_ProgramForce(ch, prog);
			else if (g->flags & LPS_SFX_F_CLEAR)
				/* No timbre to select, but the channel still has
				 * to be emptied. 0x7F is above the patch range:
				 * the synth silences and then rejects it, so the
				 * instrument survives. */
				LPS_ProgramForce(ch, 0x7F);
			voice[ch].sounding = 0;
		}
	}

	for (i = 0; i < g->count; i++) {
		const lps_sfxnote_t *n = &bank->notes[g->first + i];
		pending_t *p = &pending[n_pending++];
		uint8_t ch = resolve_ch(chans, LPS_SFXCH(n));

		p->at_tick = (uint16_t) (now_tick + 1 + n->at8);
		p->ch = (ch == 0xFF) ? 0xFF : ch;
		p->op = LPS_SFXOP(n);
		p->arg = n->arg;
		p->gate8 = n->gate8;
	}
	return 1;
}

void LPS_SfxTick(uint16_t elapsed_ms)
{
	uint8_t i;
	uint8_t ch;

	accum_ms = (uint16_t) (accum_ms + elapsed_ms);
	if (accum_ms < LPS_SFX_TICK_MS)
		return;
	accum_ms = (uint16_t) (accum_ms - LPS_SFX_TICK_MS);
	now_tick++;

	/* Release anything whose gate has run out, before striking anything new.
	 * A channel holds one note, and the note-off has to name the note it is
	 * ending -- so the release has to happen while we still know what it is. */
	for (ch = 0; ch < LPS_CHANNELS; ch++)
		if (voice[ch].sounding && voice[ch].release_tick &&
		    (int16_t) (now_tick - voice[ch].release_tick) >= 0)
			release_voice(ch);

	i = 0;
	while (i < n_pending) {
		pending_t *p = &pending[i];

		if ((int16_t) (now_tick - p->at_tick) < 0) {
			i++;
			continue;
		}

		if (p->ch != 0xFF) {
			switch (p->op) {
			case LPS_SFXOP_NOTE:
				/* Retriggering a channel that is still holding
				 * something means ending that first: one voice
				 * per channel here, and the old note would
				 * otherwise drone. */
				release_voice(p->ch);
				LPS_NoteOn(p->ch, p->arg);
				voice[p->ch].note = p->arg;
				voice[p->ch].sounding = 1;
				voice[p->ch].release_tick = p->gate8
					? (uint16_t) (now_tick + p->gate8)
					: 0;
				break;

			case LPS_SFXOP_LAYER:
				/* Deliberately no release first. See
				 * LPS_NoteOnLayered -- this is the loudness
				 * trick, and it is safe because the gesture
				 * cleared the channel on the way in. */
				LPS_NoteOnLayered(p->ch, p->arg);
				break;

			case LPS_SFXOP_BEND:
				LPS_Bend(p->ch,
					 (int16_t) (((int16_t) p->arg - 64) * 200 / 64));
				break;

			case LPS_SFXOP_PROG:
				LPS_Program(p->ch, p->arg);
				voice[p->ch].sounding = 0;
				break;

			default:
				break;
			}
		}

		/* Swap-remove: order in the pending list carries no meaning, and
		 * shifting the tail would cost more the busier things got.
		 *
		 * Field by field rather than a struct assignment, which gcc
		 * compiles to a memcpy at -Os. The library links into ROMs built
		 * -nolibc, so there is no memcpy to call -- and the host build
		 * links a C library, so only the cross build catches it. */
		{
			pending_t *src = &pending[--n_pending];

			pending[i].at_tick = src->at_tick;
			pending[i].ch = src->ch;
			pending[i].op = src->op;
			pending[i].arg = src->arg;
			pending[i].gate8 = src->gate8;
		}
	}
}
