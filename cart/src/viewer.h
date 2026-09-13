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
// Full-screen photo viewer.
//

#ifndef LPC_VIEWER_H
#define LPC_VIEWER_H

// Show photo `index` full screen; left/right and L/R step through the pack,
// wrapping. Requires an open pack with at least one photo. Does not return
// (Phase 1: the grid it will return to arrives in Phase 2).
void LPC_ViewerRun(unsigned index);

#endif
