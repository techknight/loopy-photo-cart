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
// Printing a photo as a sticker, with its dialogs.
//

#ifndef LPC_PRINTUI_H
#define LPC_PRINTUI_H

// The whole flow for photo `index`: an optional confirmation, the cassette
// check, "PRINTING...", the print, and the result. Draws over the
// framebuffer; the caller redraws its own screen afterwards (and re-uploads
// any sprite cells, since the print's VDP reset may lose them).
void LPC_PrintPhoto(unsigned index, int confirm);

#endif
