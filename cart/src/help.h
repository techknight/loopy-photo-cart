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
// The help popover (D): every control, the website, and the copyright.
//

#ifndef LPC_HELP_H
#define LPC_HELP_H

// Draw the popover over the framebuffer and wait until it is dismissed (D, B
// or A). The caller redraws its own screen afterwards.
void LPC_HelpShow(void);

#endif
