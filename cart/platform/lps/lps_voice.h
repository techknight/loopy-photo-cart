/* lps_voice.h -- who gets which channel.
 *
 * Copyright (C) 2026 loopy-soundlib contributors
 * Licensed under the GNU General Public License, version 2 or later.
 *
 *
 * Channel ownership is fixed at LPS_Init and never moves: music keeps its
 * channels and effects play only on theirs. Letting an effect borrow a music
 * channel was measured to destroy **four music notes per effect**, because a
 * program change silences a whole channel and restoring it is exactly as
 * destructive as taking it. Not one of the five commercial Loopy games with sound
 * effects borrows -- they all reserve a channel.
 *
 * What this module decides is the question that remains: when two effects want
 * the same channel at the same moment, which one plays?
 */

#ifndef LPS_VOICE_H
#define LPS_VOICE_H

#include <stdint.h>

#include "lps_midi.h"

/* Suggested priorities. Any uint8_t works; these bands have proven out on
 * hardware across thirty-odd effects and generalise well.
 *
 * Higher wins. **Equal also wins**, which is deliberate: a repeated effect should
 * retrigger cleanly rather than be refused because a copy of itself is still
 * sounding. */
#define LPS_PRI_BEAT 0     /* metronomic, ignorable                */
#define LPS_PRI_AMBIENT 1  /* background texture                   */
#define LPS_PRI_UI 2       /* menu movement, confirms              */
#define LPS_PRI_PICKUP 3   /* the player did something good        */
#define LPS_PRI_HAZARD 4   /* the player should look up            */
#define LPS_PRI_FATAL 5    /* nothing may talk over this           */

void LPS_VoiceInit(uint8_t music_mask, uint8_t sfx_mask, uint8_t mode);

/* Try to take every channel in chmask, for prio, until now + hold_ms.
 *
 * All or nothing: if any channel in the mask is held by something of higher
 * priority, nothing is taken and this returns 0. A half-played effect is not a
 * quieter effect, it is a bug that sounds like one -- so a gesture that cannot
 * have all its channels does not start.
 *
 * Channels outside the effects mask can never be claimed. Music keeps what it was
 * given at init, permanently.
 */
int LPS_VoiceClaim(uint8_t chmask, uint8_t prio, uint16_t hold_ms);

/* Hand channels back early. Claims also lapse on their own when hold_ms expires,
 * so this is only needed when an effect ends sooner than it said it would. */
void LPS_VoiceRelease(uint8_t chmask);

/* Advance the claim clock. Called from LPS_Tick. */
void LPS_VoiceTick(uint16_t elapsed_ms);

/* 1 if ch is an effects channel that is currently claimed. */
int LPS_VoiceBusy(uint8_t ch);

/* The priority currently holding ch, or 0 if free. */
uint8_t LPS_VoicePriority(uint8_t ch);

/* Masks as configured. Music never changes; effects channels are the only ones
 * this module arbitrates. */
uint8_t LPS_VoiceMusicMask(void);
uint8_t LPS_VoiceSfxMask(void);

/* How many claims were refused because something louder held a channel. A tuning
 * signal: a large number means the effects channels are oversubscribed and the
 * bank's priorities want revisiting. */
uint16_t LPS_VoiceRefused(void);

#endif /* LPS_VOICE_H */
