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
// Sound: background music and two effects on the uPD937 synth, through the
// sound driver in platform/lps.
//
// Music plays on console channels 0, 1 and 3; the effects have channel 2 to
// themselves, so an effect never cuts a music note.
//

#ifndef __LP_SOUND_H__
#define __LP_SOUND_H__

// Bring the synth up and start the music. Call before LP_ClockInit starts
// the tick that drives it.
void LP_SoundInit(void);

// Called from the ITU1 handler every tick.
void LP_SoundTick(void);

// Called once a frame from the main line (LP_VideoPresent does it): moves the
// playlist on when a song ends.
void LP_SoundFrame(void);

// Release every sounding note and hold new ones back (around a blocking
// print), then carry on.
void LP_SoundSuspend(void);
void LP_SoundResume(void);

// The two effects: a tick for d-pad movement and a chirp for any button.
enum {
	LP_SFX_MOVE,
	LP_SFX_BUTTON,
};
void LP_SfxPlay(unsigned id);

// Music on or off. Effects stay on.
void LP_MusicToggle(void);

#endif
