/* lps_sfx.h -- sound effects.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 *
 * The Loopy has no sound-effect hardware. An effect is a short MIDI phrase on the
 * same synth as the music, so this module is a tiny sequencer with different
 * priorities: it must start instantly, never interfere with the song, and cost
 * almost nothing.
 *
 * The design follows what commercial Loopy games actually do, which was recovered
 * by driving seven of them and decoding the MIDI. Three findings shape it:
 *
 *   1. **Reserve a channel; never borrow one.** Not one commercial game steals a
 *      music channel, and measurement shows why: four music notes destroyed per
 *      effect, because a program change silences a whole channel and the restore
 *      costs exactly as much as the steal.
 *
 *   2. **Clear the channel, do not release the notes.** Across 126 effect
 *      note-ons in retail games there were 2 note-offs. The idiom is a program
 *      change before each effect -- which silences whatever was there -- and then
 *      notes that are simply never released. Two bytes, no bookkeeping.
 *
 *   3. **Doubling a note is the volume control.** 39% of retail effects strike
 *      the same note twice with no release between. Two voice pairs on one sample
 *      is roughly double the amplitude, which on a synth with binary velocity is
 *      one of the only loudness controls there is.
 *
 * Gate length is the other one: releasing early cuts the envelope before it has
 * finished rising, so a short gate is genuinely a quieter hit.
 */

#ifndef LPS_SFX_H
#define LPS_SFX_H

#include <stdint.h>

#include "lps_voice.h"

/* Effects are scheduled in 8 ms units. That is fine enough that no one can hear
 * the quantisation, coarse enough that an onset and a gate both fit in a byte --
 * two seconds of range each. */
#define LPS_SFX_TICK_MS 8

/* Notes waiting to be struck across all playing gestures. */
#ifndef LPS_SFX_MAX_PENDING
#define LPS_SFX_MAX_PENDING 16
#endif

/* One step of a gesture. Four bytes. */
typedef struct {
	uint8_t at8;   /* onset from gesture start, 8 ms units */
	uint8_t opch;  /* (op << 4) | channel index within the gesture's mask */
	uint8_t arg;   /* NOTE: note | LAYER: note | BEND: MSB | PROG: program */
	uint8_t gate8; /* NOTE only: 8 ms units. 0 = never release. */
} lps_sfxnote_t;

#define LPS_SFXOP_NOTE 0
/* Strike again without releasing -- the loudness trick. Only legal on a channel
 * the gesture has cleared; see LPS_NoteOnLayered. */
#define LPS_SFXOP_LAYER 1
#define LPS_SFXOP_BEND 2
#define LPS_SFXOP_PROG 3

#define LPS_SFXOP(n) ((uint8_t) ((n)->opch >> 4))
#define LPS_SFXCH(n) ((uint8_t) ((n)->opch & 0x0F))
#define LPS_SFXNOTE_MAKE(op, ch) ((uint8_t) (((op) << 4) | ((ch) & 0x0F)))

/* One effect. Eight bytes.
 *
 * `progset` indexes a shared table of four programs rather than carrying them
 * inline. That keeps the record small, but the real reason is that it makes
 * "these effects share a timbre setup" explicit -- and effects that share a
 * progset need no program change between them, which matters because a program
 * change is a 6 ms channel-wide silence.
 */
typedef struct {
	uint16_t first;    /* index into the note table */
	uint8_t count;
	uint8_t priority;  /* LPS_PRI_*; higher wins, equal also wins */
	uint8_t chmask;    /* channels this gesture needs, all or nothing */
	uint8_t progset;   /* index into lps_sfx_progsets */
	uint8_t flags;
	uint8_t dur8;      /* total length, 8 ms units */
} lps_gesture_t;

/* Clear each channel before playing, by re-sending its program.
 *
 * On by default and almost always what you want: it releases whatever the last
 * effect left sounding, which is what makes LPS_SFXOP_LAYER safe. Turn it off
 * only for an effect layered deliberately on top of another. */
#define LPS_SFX_F_CLEAR 0x01

/* The tables a bank provides, written by hand or generated. */
typedef struct {
	const lps_sfxnote_t *notes;
	uint16_t n_notes;
	const lps_gesture_t *gestures;
	uint16_t n_gestures;
	const uint8_t *progsets; /* n_progsets * 4 programs, 0xFF = leave alone */
	uint8_t n_progsets;
	uint8_t pad;
} lps_sfxbank_t;

/* Install a bank. The channels it plays on come from LPS_Init's plan. */
void LPS_SfxSetBank(const lps_sfxbank_t *bank);

/* Start an effect. Returns 1 if it began, 0 if it was refused -- because a
 * louder effect holds a channel it needs, or because too many are already
 * queued. A refusal is normal and needs no handling. */
int LPS_SfxPlay(uint16_t id);

/* Stop everything and release the channels. */
void LPS_SfxSilence(void);

/* Master switch. Turning effects off releases anything sounding. */
void LPS_SfxEnable(int on);

/* Effects refused for want of a channel, and for want of a pending slot. Tuning
 * signals: a large refusal count means the bank's priorities want revisiting. */
uint16_t LPS_SfxRefused(void);

/* Called from LPS_Tick. */
void LPS_SfxTick(uint16_t elapsed_ms);

#endif /* LPS_SFX_H */
