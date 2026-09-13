/* lps_midi.h -- the byte layer: queue MIDI, clock it out one byte per tick.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 * The sounding-note bitmaps are adapted from LoopyDOOM's
 * platform/i_music_loopy.c (GPL-2.0-or-later). See NOTICE.md.
 *
 *
 * This is the only layer that touches the wire. Everything above it -- the
 * sequencer, the effect engine, the channel arbiter -- speaks in notes and lets
 * this decide what actually gets sent.
 *
 * Three things it knows that callers should not have to:
 *
 *   1. A duplicate note-on strands a voice forever. The synth's note-off stops at
 *      the first voice matching the pitch, so a second copy of the same note on
 *      the same channel can never be turned off again. LPS_NoteOn silently emits
 *      the note-off first. Costs two bytes; the alternative is a permanent drone.
 *
 *   2. A program change silences the whole channel. It is not "the next note uses
 *      the new patch" -- every voice is killed and the allocator is reset. So a
 *      redundant change is an audible glitch (suppressed here), and any change at
 *      all invalidates what we believed was sounding (cleared here).
 *
 *   3. Under pressure, a note-on is the right thing to lose and a note-off never
 *      is. Two different free-space thresholds enforce that.
 *
 * All emitters are safe to call from the main line while the tick runs in an
 * interrupt: the queue is single-producer, single-consumer.
 */

#ifndef LPS_MIDI_H
#define LPS_MIDI_H

#include <stdint.h>

/* Bytes of queue. Must be a power of two and must fit a uint8_t index.
 *
 * Buffering is latency: at one byte per tick, a backlog of N bytes is N/500 of a
 * second before the last one reaches the synth.
 *
 * 64 is measured, not guessed. Replaying eleven MIDI streams captured from
 * commercial Loopy games (spike/d3_soak/RESULTS.md) puts the worst high-water at
 * 33 of the 63 usable bytes -- comfortable margin, nothing wasted. Below this it
 * degrades in order: 32 starts dropping note-ons and the song plays with gaps; 16
 * starts dropping note-OFFS, and a lost note-off on this synth is a note that
 * never stops. Above it nothing improves at all -- 128 and 256 reach the same
 * high-water of 33, because that is simply what the content demands. The only
 * thing extra capacity buys is permission to fall further behind.
 */
#ifndef LPS_QUEUE_SIZE
#define LPS_QUEUE_SIZE 64
#endif

/* Free-space floors. A note-on needs 3 bytes worst case and a note-off 2, so the
 * gap between these two numbers is deliberate headroom reserved for note-offs:
 * when the queue tightens, new notes stop but held notes can still be released.
 */
#define LPS_Q_MIN_FOR_NOTEON 8
#define LPS_Q_MIN_FOR_NOTEOFF 3

#define LPS_CHANNELS 4

/* Marker for "we have not sent a program on this channel yet", and also what the
 * cache is reset to after anything that invalidates it. */
#define LPS_PROG_UNKNOWN 0xFF

void LPS_MidiInit(void);

/* Bytes that can still be queued. */
int LPS_MidiFree(void);

/* High-water mark of queue occupancy since init -- what D3 sizes the queue from,
 * and what a game can assert on in its own soak test. */
uint8_t LPS_MidiHighWater(void);

/* Messages dropped for want of space, by kind. A nonzero note-off count is a bug
 * in the caller's pacing, not a tuning matter: it means something is sounding
 * that we asked to stop. */
uint16_t LPS_MidiDroppedNoteOns(void);
uint16_t LPS_MidiDroppedNoteOffs(void);

/* All return 1 if the message was queued, 0 if it was refused. A refusal is not
 * an error -- it is the drop policy working. */
int LPS_NoteOn(uint8_t ch, uint8_t note);
int LPS_NoteOff(uint8_t ch, uint8_t note);
int LPS_Program(uint8_t ch, uint8_t prog);

/* Send a program change even if that program is already in force.
 *
 * LPS_Program suppresses a redundant change because on this synth it is not a
 * no-op -- it silences every voice on the channel for about 6 ms. This exists
 * for the case where that silence is exactly what is wanted.
 *
 * That case is a sound-effect channel. Re-sending the program does two jobs at
 * once for two bytes: it stops whatever the last effect left sounding (including
 * a layered copy that no note-off could reach) and it selects the timbre for the
 * next one. Every commercial Loopy game surveyed uses this as its note-off; see
 * docs/retail-sfx.md.
 *
 * Never call this on a music channel. It will cut the arrangement off mid-phrase.
 */
int LPS_ProgramForce(uint8_t ch, uint8_t prog);

/* Strike a note that is already sounding, WITHOUT releasing it first.
 *
 * This deliberately does the thing LPS_NoteOn exists to prevent, because it is
 * also a technique. Two voice pairs playing the same sample at once is roughly
 * twice the amplitude, and on a synth with binary velocity and no volume
 * controller that is one of the very few ways to make one sound louder than
 * another. Commercial Loopy games use it constantly: 39% of the sound effects
 * surveyed in docs/retail-sfx.md are a note struck twice.
 *
 * The catch is real and unavoidable. The synth's note-off stops at the first
 * voice matching the pitch, so the second copy can never be released by note
 * number -- LPS_NoteOff will silence one and leave the other sounding forever.
 *
 * **The only way to reclaim it is to clear the whole channel**, with
 * LPS_Program or LPS_MidiPanic. So this is safe under exactly one discipline,
 * which is the one the retail games follow and lps_sfx enforces:
 *
 *     a channel that carries doubled notes is cleared before every use and is
 *     never released note by note.
 *
 * Do not call this from music. Use it for effects on a channel you own.
 */
int LPS_NoteOnLayered(uint8_t ch, uint8_t note);

/* Bend in cents, clamped to the synth's real range of +/-200 (measured from the
 * sound ROM's rate table; see docs/instruments.md). Resolution is 7 bits, about
 * 3 cents -- the synth discards the low data byte. Under running status a repeat
 * bend costs 2 bytes, the same as a note, which is what makes sweeps affordable.
 */
int LPS_Bend(uint8_t ch, int16_t cents);

/* Release every note this channel is known to be holding, one explicit note-off
 * each. There is no shortcut: the synth ignores CC 123. */
void LPS_SilenceChannel(uint8_t ch);

/* Stop all four channels using the program >= 110 trick: the synth runs its
 * silencing loop and then rejects the program, leaving the patch untouched. Eight
 * bytes to stop everything, versus one note-off per sounding note.
 *
 * This bypasses the queue's drop policy -- a panic that gets dropped is not a
 * panic. It also resets the program cache, because the synth's idea of the
 * current patch and ours no longer agree.
 */
void LPS_MidiPanic(void);

uint8_t LPS_Sounding(uint8_t ch, uint8_t note);
uint8_t LPS_PolyNow(uint8_t ch);
uint8_t LPS_CurProgram(uint8_t ch);

/* Move one byte to the wire. Call from the tick and nowhere else. */
void LPS_MidiTxTick(void);

#endif /* LPS_MIDI_H */
