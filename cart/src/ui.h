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
// Small drawing helpers shared by the screens.
//

#ifndef LPC_UI_H
#define LPC_UI_H

#include <stdint.h>

// Stage the reserved UI palette slots (LPC_UI_*), and slot 0 as the UI
// background, for screens that show no image.
void LPC_UiPalette(void);

// Fill a rectangle of the framebuffer with one colour, clipped.
void LPC_FillRect(int x, int y, int w, int h, uint8_t colour);

#endif
