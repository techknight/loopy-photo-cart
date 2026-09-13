/* lps_inst.h -- instrument tables read out of the Loopy's sound ROM.
 *
 * Facts read out of the Loopy's wavetable ROM: which presets are layered
 * (and so cost double the voices), how many notes each channel can actually
 * sound, and where the drum kit's slots are.
 *
 * Instrument NAMES are not in the ROM. The named LPS_PROG_* constants at the
 * end are by-ear identifications.
 *
 * Licence: GPL-2.0-or-later. See COPYING.md.
 */

#ifndef LPS_INST_H
#define LPS_INST_H

#include <stdint.h>

#define LPS_PROG_COUNT 110 /* 0..109 are patches */
#define LPS_NOTE_LO 36
#define LPS_NOTE_HI 96

/* Programs >= this are not patches. The synth runs its channel-silencing
 * loop and then returns without changing the instrument, which makes this
 * the cheapest possible panic: two bytes to stop a channel dead. It does
 * invalidate any cached program number, so reset the cache after using it. */
#define LPS_PROG_ALL_OFF 110

/* ---- Layered presets ---------------------------------------------------
 *
 * A layered preset allocates four raw voices per note instead of two, which
 * halves the channel's polyphony. On channel 2 -- four raw voices -- that
 * means exactly one note at a time.
 *
 * Layered: 2, 6, 8, 31, 33, 44, 45, 64, 67, 88, 105, 109
 */
#define LPS_PROG_LAYERED_MASK { 0x80000144U, 0x00003002U, 0x01000009U, 0x00002200U }

static const uint32_t lps_prog_layered[4] = LPS_PROG_LAYERED_MASK;

static inline int LPS_ProgLayered(uint8_t prog)
{
	if (prog >= LPS_PROG_COUNT)
		return 0;
	return (lps_prog_layered[prog >> 5] >> (prog & 31)) & 1;
}

static inline uint8_t LPS_ProgVoicesPerNote(uint8_t prog)
{
	return LPS_ProgLayered(prog) ? 4 : 2;
}

/* ---- Polyphony ---------------------------------------------------------
 *
 * Notes each channel can sound at once, indexed [mode][channel][layered].
 * Mode order matches SOUND_CHANS_*: 4ch, 3ch_rhythm, 3ch, 1ch.
 *
 * Channel 2 has only four raw voices, so a layered preset makes it
 * monophonic. That is the trap: programs 64 (brass) and 105 (synth) are
 * both layered.
 */
static const uint8_t lps_poly_cap[4][4][2] = {
	{ {6, 3}, {4, 2}, {2, 1}, {4, 2} }, /* 4ch */
	{ {6, 3}, {4, 2}, {2, 1}, {0, 0} }, /* 3ch_rhythm */
	{ {6, 3}, {4, 2}, {2, 1}, {0, 0} }, /* 3ch */
	{ {12, 6}, {0, 0}, {0, 0}, {0, 0} }, /* 1ch */
};

static inline uint8_t LPS_PolyCap(uint8_t mode, uint8_t ch, uint8_t prog)
{
	if (mode > 3 || ch > 3)
		return 0;
	return lps_poly_cap[mode][ch][LPS_ProgLayered(prog)];
}

/* ---- The drum kit ------------------------------------------------------
 *
 * Program 39 is the only 16-zone program in the ROM, which is what makes it
 * a kit: the note picks a sample slot rather than a pitch. There are 16
 * slots. The constants below name the LOWEST note of each zone, because a
 * sample stretched least sounds fullest -- but any note inside a zone's
 * range plays the same drum, transposed.
 *
 * Two notes from the same zone are the same drum, however far apart they
 * look.
 */
#define LPS_PROG_KIT 39

#define LPS_KIT_Z0  36 /* notes 36-38  sample 152, +24 semitones */
#define LPS_KIT_Z1  39 /* notes 39-42  sample 153, +23 semitones */
#define LPS_KIT_Z2  43 /* notes 43-44  sample 154, +23 semitones */
#define LPS_KIT_Z3  45 /* notes 45-48  sample 155, +23 semitones */
#define LPS_KIT_Z4  49 /* notes 49-51  sample 156, +23 semitones */
#define LPS_KIT_Z5  52 /* notes 52-55  sample 158, +23 semitones */
#define LPS_KIT_Z6  56 /* notes 56-61  sample 162, +20 semitones */
#define LPS_KIT_Z7  62 /* notes 62-64  sample 163, +24 semitones */
#define LPS_KIT_Z8  65 /* notes 65-66  sample 164, +22 semitones */
#define LPS_KIT_Z9  67 /* notes 67-68  sample 165, +19 semitones */
#define LPS_KIT_ZA  69 /* notes 69-70  sample 166, +14 semitones */
#define LPS_KIT_ZB  71 /* notes 71-72  sample 167, +24 semitones */
#define LPS_KIT_ZC  73 /* notes 73-80  sample 170, +20 semitones */
#define LPS_KIT_ZD  81 /* notes 81-86  sample 175, +24 semitones */
#define LPS_KIT_ZE  87 /* notes 87-95  sample 116, +16 semitones */
#define LPS_KIT_ZF  96 /* notes 96     sample 177, +24 semitones */

#define LPS_KIT_SLOT_COUNT 16
static const uint8_t lps_kit_slots[LPS_KIT_SLOT_COUNT] = {
	36, 39, 43, 45, 49, 52, 56, 62, 65, 67, 69, 71, 73, 81, 87, 96
};

/* Fully unpitched programs -- every note plays at a fixed rate. */
#define LPS_PROG_UNPITCHED_95 95
#define LPS_PROG_UNPITCHED_97 97

/* ---- Pitch bend --------------------------------------------------------
 *
 * Bend depth is a ROM table, not arithmetic, so this range is measured:
 * -64..+64 pitch units over the full 7-bit MSB sweep, at 32 units per
 * semitone = -2.00..+2.00 semitones, monotonic.
 *
 * The synth reads only the MSB data byte and discards the LSB, so the real
 * resolution is 7 bits -- about 3.1 cents per step.
 */
#define LPS_BEND_UNITS_MAX 64
#define LPS_BEND_SEMITONES_MAX 2 /* rounded; see the comment above */

/* ---- Instrument names --------------------------------------------------
 *
 * Not from the ROM. These are by-ear identifications; where listeners heard
 * different things, each description is kept.
 */
#define LPS_PROG_PIANO          1 /* piano | harpsichord | sting layer */
#define LPS_PROG_MUSIC_BOX      2
#define LPS_PROG_GUITAR         4 /* guitar | distortion guitar */
#define LPS_PROG_ORGAN         16
#define LPS_PROG_BASS_19       19
#define LPS_PROG_BASS_41       41
#define LPS_PROG_BRASS         64 /* brass | brass/saw -- LAYERED */
#define LPS_PROG_BASS          87 /* bass | synth bass */
#define LPS_PROG_BOWED_PAD     90 /* usable as a lead */
#define LPS_PROG_KALIMBA       96
#define LPS_PROG_STRINGS      100 /* a close match for the SC-55's strings */
#define LPS_PROG_SYNTH2       105 /* synth 2 | synth brass -- LAYERED */
#define LPS_PROG_SYNTH        108

#endif /* LPS_INST_H */
