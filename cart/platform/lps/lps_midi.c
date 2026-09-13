/* lps_midi.c -- MIDI byte queue and transmitter.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 * The sounding-note bitmaps are adapted from LoopyDOOM's
 * platform/i_music_loopy.c (GPL-2.0-or-later). See NOTICE.md.
 *
 *
 * Bandwidth is the whole problem. One byte leaves per tick, so at the default
 * 500 Hz that is 500 bytes/second against a wire that could carry 3125. Three
 * things close the gap between what music wants and what the tick delivers, in
 * this order of effectiveness:
 *
 *   - running status, so a note on the same channel costs two bytes, not three
 *   - suppressing redundant program changes, which are both expensive and
 *     destructive
 *   - dropping note-ons before note-offs when the queue backs up
 *
 * Retail Loopy games use running status too -- five of the eight sampled in
 * docs/instruments.md, one of them for 88% of its messages -- so it is proven on
 * this synth and not just on paper.
 *
 *
 * Concurrency
 * -----------
 * The queue is single-producer, single-consumer. The main line only ever advances
 * queue_head; the tick only ever advances queue_tail. Both are volatile uint8_t,
 * so each update is a single byte store and an interrupt taken between
 * instructions can never see a half-written index. That is why there is no
 * masking here, and it is the same argument loopy-glider ships on.
 *
 * The state that is NOT protected by that argument -- running_status, the
 * sounding bitmaps, cur_prog -- is touched only by the emitters, never by the
 * tick. Keep it that way.
 */

#include "lps_port.h"
#include "lps_midi.h"

/* Deliberately no <string.h>.
 *
 * The homebrew template links -nolibc, and a game that has gone to the trouble
 * of not needing a C library should not have to take one on because it wanted
 * sound. Everything here that would have called memset is four words wide, so
 * the hand-written loop is no worse anyway. */

#if (LPS_QUEUE_SIZE & (LPS_QUEUE_SIZE - 1)) != 0
#error "LPS_QUEUE_SIZE must be a power of two"
#endif
#if LPS_QUEUE_SIZE > 256
#error "LPS_QUEUE_SIZE must fit a uint8_t index"
#endif
#if LPS_QUEUE_SIZE < 16
#error "LPS_QUEUE_SIZE below 16 cannot hold the note-off reserve"
#endif

#define QMASK (LPS_QUEUE_SIZE - 1)

/* Programs at or above this are not patches. Sending one silences the channel and
 * leaves the instrument alone -- see docs/hardware.md. */
#define PROG_ALL_OFF 0x7F

static uint8_t queue[LPS_QUEUE_SIZE];
static volatile uint8_t queue_head;
static volatile uint8_t queue_tail;

static uint8_t running_status;
static uint8_t ready;

/* What we believe is sounding, indexed by raw MIDI note.
 *
 * 128 bits per channel rather than the 61 the synth can actually pitch, because
 * the synth folds out-of-range notes for pitch but matches note-off against the
 * raw number it was sent. Tracking the raw number is what keeps on/off pairing
 * honest when a caller sends note 24.
 */
static uint32_t sounding[LPS_CHANNELS][4];
static uint8_t poly_now[LPS_CHANNELS];
static uint8_t cur_prog[LPS_CHANNELS];

static uint8_t high_water;
static uint16_t dropped_on;
static uint16_t dropped_off;

/* ---- queue ---------------------------------------------------------------- */

static int queue_free(void)
{
	int used = (int) ((queue_head - queue_tail) & QMASK);

	return LPS_QUEUE_SIZE - 1 - used;
}

/* Callers reserve worst-case space before writing their first byte, so this never
 * has to refuse mid-message. A truncated message would desync the synth's parser
 * permanently, where a missing one costs a single click. */
static void put(uint8_t b)
{
	uint8_t next = (uint8_t) ((queue_head + 1) & QMASK);

	if (next == queue_tail)
		return;
	queue[queue_head] = b;
	queue_head = next;
}

static void note_used(void)
{
	uint8_t used = (uint8_t) ((queue_head - queue_tail) & QMASK);

	if (used > high_water)
		high_water = used;
}

/* Emit a status byte only when it differs from the last one sent. Note-off is
 * encoded as note-on velocity 0 precisely so that it shares this status byte with
 * note-on and costs two bytes instead of three. */
static void status(uint8_t s)
{
	if (s != running_status) {
		put(s);
		running_status = s;
	}
}

/* ---- bitmap --------------------------------------------------------------- */

static uint8_t bit_get(uint8_t ch, uint8_t note)
{
	return (uint8_t) ((sounding[ch][note >> 5] >> (note & 31)) & 1);
}

static void bit_set(uint8_t ch, uint8_t note)
{
	if (!bit_get(ch, note)) {
		sounding[ch][note >> 5] |= (uint32_t) 1 << (note & 31);
		poly_now[ch]++;
	}
}

static void bit_clear(uint8_t ch, uint8_t note)
{
	if (bit_get(ch, note)) {
		sounding[ch][note >> 5] &= ~((uint32_t) 1 << (note & 31));
		poly_now[ch]--;
	}
}

/* Forget everything about a channel's notes without emitting anything.
 *
 * This is what a program change does, and getting it wrong is subtle: the synth
 * has already killed those voices, so sending note-offs for them afterwards is
 * not merely wasteful. If something else has since claimed the channel and is
 * holding the same pitch, a stale note-off silences the wrong sound.
 */
static void bit_clear_all(uint8_t ch)
{
	int w;

	for (w = 0; w < 4; w++)
		sounding[ch][w] = 0;
	poly_now[ch] = 0;
}

/* ---- lifecycle ------------------------------------------------------------ */

void LPS_MidiInit(void)
{
	uint8_t ch;

	queue_head = 0;
	queue_tail = 0;
	running_status = 0;
	high_water = 0;
	dropped_on = 0;
	dropped_off = 0;

	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		bit_clear_all(ch);
		cur_prog[ch] = LPS_PROG_UNKNOWN;
	}

	ready = 1;
}

int LPS_MidiFree(void)
{
	return queue_free();
}

uint8_t LPS_MidiHighWater(void)
{
	return high_water;
}

uint16_t LPS_MidiDroppedNoteOns(void)
{
	return dropped_on;
}

uint16_t LPS_MidiDroppedNoteOffs(void)
{
	return dropped_off;
}

uint8_t LPS_Sounding(uint8_t ch, uint8_t note)
{
	if (ch >= LPS_CHANNELS)
		return 0;
	return bit_get(ch, (uint8_t) (note & 0x7F));
}

uint8_t LPS_PolyNow(uint8_t ch)
{
	return ch < LPS_CHANNELS ? poly_now[ch] : 0;
}

uint8_t LPS_CurProgram(uint8_t ch)
{
	return ch < LPS_CHANNELS ? cur_prog[ch] : LPS_PROG_UNKNOWN;
}

/* ---- emitters ------------------------------------------------------------- */

/* The note-off half of a note-on/note-off pair, factored out because note-on has
 * to be able to call it: see the duplicate-note rule below. Assumes space has
 * already been reserved. */
static void emit_off(uint8_t ch, uint8_t note)
{
	status((uint8_t) (0x90 | ch));
	put(note);
	put(0x00);
	bit_clear(ch, note);
}

int LPS_NoteOn(uint8_t ch, uint8_t note)
{
	if (!ready || ch >= LPS_CHANNELS)
		return 0;
	note &= 0x7F;

	/* Worst case is a status byte plus a note-off pair plus a note-on pair,
	 * because of the duplicate rule below. Reserve for it up front rather
	 * than discovering halfway through that the queue is full. */
	if (queue_free() < LPS_Q_MIN_FOR_NOTEON) {
		dropped_on++;
		return 0;
	}

	/* The duplicate-note rule.
	 *
	 * The synth's note-off scans its voices and stops at the first one whose
	 * pitch matches. So if this (channel, note) is already sounding, the copy
	 * we are about to start could never be turned off again -- nothing decays
	 * it, nothing reclaims it, and it drones until the channel gets a program
	 * change. There is no way to fix it after the fact.
	 *
	 * Two bytes to release it first is cheap insurance, and it also gives the
	 * retrigger the clean attack the caller almost certainly wanted.
	 */
	if (bit_get(ch, note))
		emit_off(ch, note);

	status((uint8_t) (0x90 | ch));
	put(note);
	put(0x40); /* Velocity is binary here; any nonzero value means "on". */
	bit_set(ch, note);
	note_used();
	return 1;
}

int LPS_NoteOnLayered(uint8_t ch, uint8_t note)
{
	if (!ready || ch >= LPS_CHANNELS)
		return 0;
	note &= 0x7F;

	if (queue_free() < LPS_Q_MIN_FOR_NOTEON) {
		dropped_on++;
		return 0;
	}

	status((uint8_t) (0x90 | ch));
	put(note);
	put(0x40);

	/* The bitmap records that this note is sounding, but it cannot record
	 * that it is sounding *twice* -- a bit has no room for a count. So after
	 * this the driver's idea of the channel is deliberately incomplete, and
	 * only a channel clear can make it true again. That is the whole reason
	 * the header insists this is only for channels that get cleared.
	 *
	 * Setting the bit anyway is still right: it keeps LPS_SilenceChannel
	 * emitting a release for the note, which silences one of the two copies
	 * and is better than neither. */
	bit_set(ch, note);
	note_used();
	return 1;
}

int LPS_NoteOff(uint8_t ch, uint8_t note)
{
	if (!ready || ch >= LPS_CHANNELS)
		return 0;
	note &= 0x7F;

	/* Not sounding, as far as we know -- so either it already stopped or a
	 * program change took it. Either way the synth has nothing to release and
	 * sending a note-off could hit someone else's note. Report success:
	 * the caller's intent (this note is not sounding) is satisfied. */
	if (!bit_get(ch, note))
		return 1;

	if (queue_free() < LPS_Q_MIN_FOR_NOTEOFF) {
		/* This should not happen -- the note-on threshold is higher
		 * precisely to keep room here. If it does, something is emitting
		 * without pacing, and the result is a stuck note. */
		dropped_off++;
		return 0;
	}

	emit_off(ch, note);
	note_used();
	return 1;
}

static int emit_program(uint8_t ch, uint8_t prog)
{
	if (queue_free() < LPS_Q_MIN_FOR_NOTEON)
		return 0;

	put((uint8_t) (0xC0 | ch));
	put(prog);

	/* Program change is not a channel message for running-status purposes as
	 * far as we are concerned -- the next note must re-send its status byte,
	 * because the last status on the wire is now 0xCn. */
	running_status = 0;

	/* Everything on this channel is already gone. Forget it without emitting
	 * note-offs; see bit_clear_all. */
	bit_clear_all(ch);

	/* Above the patch range the synth silences and then rejects, so the
	 * instrument is unchanged and our cache would be a lie. */
	cur_prog[ch] = (prog >= PROG_ALL_OFF) ? LPS_PROG_UNKNOWN : prog;

	note_used();
	return 1;
}

int LPS_Program(uint8_t ch, uint8_t prog)
{
	if (!ready || ch >= LPS_CHANNELS)
		return 0;
	prog &= 0x7F;

	/* A redundant program change is not a no-op on this synth: it silences
	 * every voice on the channel over about 6 ms and resets the allocator.
	 * Suppressing it is a correctness measure, not an optimisation. */
	if (cur_prog[ch] == prog)
		return 1;

	return emit_program(ch, prog);
}

int LPS_ProgramForce(uint8_t ch, uint8_t prog)
{
	if (!ready || ch >= LPS_CHANNELS)
		return 0;
	return emit_program(ch, (uint8_t) (prog & 0x7F));
}

int LPS_Bend(uint8_t ch, int16_t cents)
{
	int32_t msb;

	if (!ready || ch >= LPS_CHANNELS)
		return 0;

	/* Measured from the sound ROM: the bend table spans exactly +/-2
	 * semitones, linearly. Anything beyond that is not reachable, so clamp
	 * rather than wrap. */
	if (cents > 200)
		cents = 200;
	if (cents < -200)
		cents = -200;

	msb = 64 + ((int32_t) cents * 64) / 200;
	if (msb < 0)
		msb = 0;
	if (msb > 127)
		msb = 127;

	if (queue_free() < LPS_Q_MIN_FOR_NOTEON)
		return 0;

	status((uint8_t) (0xE0 | ch));
	/* The synth reads the MSB only and discards this byte, but MIDI requires
	 * it and the retiming queue counts it. */
	put(0x00);
	put((uint8_t) msb);
	note_used();
	return 1;
}

void LPS_SilenceChannel(uint8_t ch)
{
	int w;

	if (!ready || ch >= LPS_CHANNELS)
		return;

	/* Walk the bits rather than sweeping 0..127. A channel holds at most six
	 * notes, so this is six iterations of the inner loop and not 128 -- which
	 * matters because this can be called from the tick. */
	for (w = 0; w < 4; w++) {
		uint32_t m = sounding[ch][w];

		while (m) {
			uint8_t b = 0;
			uint32_t low = m & (uint32_t) (-(int32_t) m);

			/* Deliberately not __builtin_ctz: this has to build on
			 * whatever compiler a downstream game is using, and six
			 * iterations of a shift loop is not worth a dependency. */
			while ((low >> b) != 1)
				b++;
			m &= m - 1;

			if (queue_free() < LPS_Q_MIN_FOR_NOTEOFF) {
				dropped_off++;
				return;
			}
			emit_off(ch, (uint8_t) ((w << 5) | b));
		}
	}
	note_used();
}

void LPS_MidiPanic(void)
{
	uint8_t ch;

	if (!ready)
		return;

	/* Deliberately not routed through LPS_Program: that would honour the
	 * drop policy, and a panic that gets dropped is not a panic. Eight bytes
	 * always fit -- the queue is at least 16.
	 *
	 * If the queue is genuinely full, these land behind whatever is already
	 * in it, which is the correct order anyway: the backlog is notes we
	 * already decided to send.
	 */
	for (ch = 0; ch < LPS_CHANNELS; ch++) {
		put((uint8_t) (0xC0 | ch));
		put(PROG_ALL_OFF);
		bit_clear_all(ch);
		cur_prog[ch] = LPS_PROG_UNKNOWN;
	}
	running_status = 0;
	note_used();
}

/* ---- transmitter ---------------------------------------------------------- */

void LPS_MidiTxTick(void)
{
	if (!ready || queue_tail == queue_head)
		return;

	LPS_PORT_TX(queue[queue_tail]);
	queue_tail = (uint8_t) ((queue_tail + 1) & QMASK);
}
