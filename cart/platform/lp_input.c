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
// Gamepad backend.
//
// The raw VDP register is read directly, not biosvar_gamepad: the BIOS
// refreshes its copy only inside bios_vsync().
//
// Note the layout READ_GAMEPAD1_RAW unpacks: detect/Start/L/R arrive in bits
// 0-3 of IO_GAMEPAD[0], A/D/C/B in bits 8-11 of the same register, and the
// d-pad in IO_GAMEPAD[1]. Masking the register with 0xFF silently drops all
// four face buttons.
//

#include "loopy.h"

#include "lp_platform.h"
#include "lp_input.h"

static volatile uint16_t pad_held;
static volatile uint16_t pad_edges;
static volatile uint8_t sampling;

void LP_InputInit(void)
{
	pad_held = 0;
	pad_edges = 0;
	sampling = 1;
}

void LP_InputPause(void)
{
	sampling = 0;
}

// After a print the scan comes back reading an idle image, which would show
// as buttons held and swallow the next presses (seen on hardware).
// Re-arming it and clearing every latch avoids both.
void LP_InputRescan(void)
{
	uint32_t sr;

	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_240P);

	sr = LP_IrqBlock();
	pad_held = 0;
	pad_edges = 0;
	sampling = 1;
	LP_IrqRestore(sr);
}

// Called from LP_ClockISR every fifth 2 ms tick, i.e. at 100 Hz.
void LP_PadSample(void)
{
	uint16_t now;

	if (!sampling)
		return;
	now = (uint16_t) READ_GAMEPAD1_RAW;

	pad_edges |= (uint16_t) (now & ~pad_held);
	pad_held = now;
}

uint16_t LP_PadHeld(void)
{
	return pad_held;
}

uint16_t LP_PadEdges(void)
{
	// The sampler could otherwise fire between the read and the clear and
	// drop a press.
	uint32_t sr = LP_IrqBlock();
	uint16_t e = pad_edges;

	pad_edges = 0;
	LP_IrqRestore(sr);

	return e;
}
