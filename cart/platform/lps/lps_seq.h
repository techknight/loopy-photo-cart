/* lps_seq.h -- music playback from a baked event stream.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 * The event-stream design and the volatile-gate discipline are adapted from
 * LoopyDOOM's platform/i_music_loopy.c (GPL-2.0-or-later). See NOTICE.md.
 *
 *
 * Songs are compiled offline by tools/lps_bake.py into a flat array of 4-byte
 * events with delta timing. Playback runs in the tick interrupt, so it is
 * unaffected by frame rate -- a game that drops to 8 fps still keeps time.
 */

#ifndef LPS_SEQ_H
#define LPS_SEQ_H

#include <stdint.h>

/* Events consumed per tick before the rest roll over to the next one.
 *
 * This is a load shedder, not a budget. A dense chord can want a dozen events at
 * once, and emitting them all would queue thirty-odd bytes -- more than half the
 * queue -- in a single interrupt, starving anything else that wanted to speak.
 * Spilling to the next tick costs 2 ms of skew that nobody can hear.
 *
 * 8 covers 99% of ticks outright across eleven streams of real commercial Loopy
 * music, with a worst observed burst of 14. See spike/d3_soak/RESULTS.md -- and
 * note that the measurement is pessimistic, because the captures it is drawn from
 * have frame resolution and so concentrate up to eight ticks' work into one.
 */
#ifndef LPS_SEQ_MAX_EV_PER_TICK
#define LPS_SEQ_MAX_EV_PER_TICK 8
#endif

/* One event. Four bytes, two-byte aligned.
 *
 *   dt   milliseconds since the previous event
 *   op   (opcode << 6) | (channel << 4) | 4 spare bits the opcode defines
 *   arg  opcode-defined
 */
typedef struct {
	uint16_t dt;
	uint8_t op;
	uint8_t arg;
} lps_ev_t;

#define LPS_OP_NOTE 0 /* op bit0 of the low nibble = on; arg = note      */
#define LPS_OP_PROG 1 /* arg = program                                   */
#define LPS_OP_CTRL 2 /* low nibble = control id; arg = value            */
#define LPS_OP_META 3 /* low nibble = meta id; arg = payload             */

#define LPS_CTRL_BEND 0 /* arg = bend MSB, 0..127, 64 = centre           */

#define LPS_META_END 0    /* stop, or loop if the song loops             */
#define LPS_META_LOOP 1   /* loop point marker (informational)           */
#define LPS_META_MARKER 2 /* arg = a marker id the game can poll         */
#define LPS_META_NOP 3    /* spacer for gaps longer than 65535 ms        */

#define LPS_EV_OP(e) ((uint8_t) ((e)->op >> 6))
#define LPS_EV_CH(e) ((uint8_t) (((e)->op >> 4) & 3))
#define LPS_EV_LOW(e) ((uint8_t) ((e)->op & 0x0F))

#define LPS_EV_MAKE(opcode, ch, low) \
	((uint8_t) (((opcode) << 6) | (((ch) & 3) << 4) | ((low) & 0x0F)))

typedef struct {
	const lps_ev_t *evs;
	uint16_t n_events;
	/* Event index to jump to at the end. 0xFFFF means the song stops. */
	uint16_t loop_event;
	/* Program each channel starts on. 0xFF leaves the channel alone, which
	 * is what a song that does not use a channel should say. */
	uint8_t prog[4];
	/* Channels this song touches. Anything outside is left untouched, so a
	 * song and a sound effect can coexist without either knowing about the
	 * other. */
	uint8_t chmask;
	uint8_t flags;
	uint8_t pad;
} lps_song_t;

#define LPS_SONG_LOOPS 0x01

void LPS_MusicPlay(const lps_song_t *song);
void LPS_MusicStop(void);
void LPS_MusicPause(int paused);

/* Fade over ms and then stop.
 *
 * Staged, because the synth has no volume control worth the name: the two
 * hardware sliders cover 4 dB and do not touch channel 0 at all. So a fade here
 * is a thinning -- fewer notes, then none, then silence. It reads as a fade
 * because a sparser arrangement sounds further away, not because anything gets
 * quieter. Do not expect a smooth ramp; the hardware cannot do one.
 */
void LPS_MusicFadeOut(uint16_t ms);

int LPS_MusicPlaying(void);
uint32_t LPS_MusicPosMs(void);

/* The last LPS_META_MARKER passed, for syncing gameplay to the music. Reading it
 * clears it. */
uint8_t LPS_MusicTakeMarker(void);

/* Called from LPS_Tick. */
void LPS_SeqTick(uint16_t elapsed_ms);

#endif /* LPS_SEQ_H */
