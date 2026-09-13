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
// Gamepad input. Sampled at 100 Hz from the ITU1 interrupt (lp_clock.c).
// Button masks are GAMEPAD_BTN_* in include/loopy/constants.h.
//

#ifndef __LP_INPUT_H__
#define __LP_INPUT_H__

#include <stdint.h>

void LP_InputInit(void);

// Buttons currently down.
uint16_t LP_PadHeld(void);

// Press edges accumulated since the last call; reading clears them.
uint16_t LP_PadEdges(void);

// Stop sampling (around a print, which shares the VDP's IO expansion with the
// controller scan), and re-arm the scan afterwards with every latch cleared.
void LP_InputPause(void);
void LP_InputRescan(void);

#endif
