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
// The grid cursor: a ring around a thumbnail, drawn with sprites so that
// moving it never touches the bitmap.
//

#ifndef LPC_CURSOR_H_INCLUDED
#define LPC_CURSOR_H_INCLUDED

// Ring thickness outside the thumbnail, and the ring's full size around a
// 64x60 thumbnail.
#define LPC_CURSOR_BORDER 4
#define LPC_CURSOR_W      72
#define LPC_CURSOR_H      68

// Build the ring's char cells and upload them. Call once.
void LPC_CursorInit(void);

// Place the ring around a thumbnail whose top-left is (x, y). Takes effect at
// the next present.
void LPC_CursorShowAround(int x, int y);

void LPC_CursorHide(void);

#endif
