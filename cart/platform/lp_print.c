//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// The sticker printer, through the boot BIOS.
//
// Adapted from LoopyDOOM's platform/loopy_print.c (ThroatyMumbo,
// GPL-2.0-or-later, https://github.com/ThroatyMumbo/LoopyDOOM), by way of the
// same author's LoopyManiac, where this sequence and its recovery steps were
// proven on hardware. See NOTICE.md.
//
// The BIOS owns the head, the motor and the sensors; the cartridge's whole job
// is to hand bios_print8bpp a 256-strided 8bpp buffer plus an RGB555 palette
// in console RAM and drive its polled state machine until it settles. LoopyMSE
// hooks the same BIOS entry point and writes a PNG, so the path is testable
// without paper (its PNG stops at 224 lines; hardware prints all 241).
//

#include <string.h>

#include "loopy.h"

#include "lp_clock.h"
#include "lp_input.h"
#include "lp_print.h"
#include "lp_sound.h"
#include "lp_video.h"

// The head-temperature poll: 0 once warm enough. Hardware-confirmed by
// LoopyDOOM; its address is in tools/loopy.ld.
extern uint32_t bios_checkPrintTemp(void);

// Zeroing these BIOS ISR-work pointers selects the synchronous, no-IRQ print
// path. They live in the low 0x100 bytes of RAM the linker reserves for the
// BIOS (tools/loopy.ld).
#define BIOS_PRINT_ISR_WORK  (*(volatile uint32_t *) 0x0900000Cu)
#define BIOS_PRINT_ISR_FLAG  (*(volatile uint32_t *) 0x09000010u)
#define BIOS_PRINT_ISR_WORK2 (*(volatile uint32_t *) 0x09000014u)

// LoopyDOOM's guards: a state machine that will not settle and a head that
// will not warm are both "give up", never "hang the console".
#define PRINT_STEP_GUARD 200000u
#define TEMP_WAIT_GUARD  20000000u

int LP_PrintSealType(void)
{
	return bios_getSealType();
}

int LP_PrintSticker(const uint8_t *pixels, const uint16_t *palette)
{
	static uint16_t pal[256]; // must live in console RAM, not ROM
	uint32_t guard = PRINT_STEP_GUARD;
	int status;

	memcpy(pal, palette, sizeof pal);

	// The printer and the controller scan share the VDP's IO expansion, and
	// neither survives the other touching it mid-flight. Sampling stops for
	// the whole print and the scan is re-armed after, on every exit path.
	LP_InputPause();

	// The synth is a separate chip: silence it, or held notes sound for the
	// whole print.
	LP_SoundSuspend();

	BIOS_PRINT_ISR_WORK = 0;
	BIOS_PRINT_ISR_FLAG = 0;
	BIOS_PRINT_ISR_WORK2 = 0;

	for (;;) {
		// The buffer is only read, but the BIOS prototype predates const.
		uint8_t r = bios_print8bpp((uint8_t *) pixels, pal, 1);

		// Only warm-up (5) is worth retrying: spin the temperature poll
		// until the head is ready, then ask again. Everything else is
		// final; retrying a refusal would fight the sensor that refused.
		if (r != LP_PRINT_HOT) {
			status = r;
			break;
		}
		{
			uint32_t tg = TEMP_WAIT_GUARD;

			while (bios_checkPrintTemp() != 0)
				if (--tg == 0)
					break;
			if (tg == 0) {
				status = LP_PRINT_HOT;
				break;
			}
		}
		if (--guard == 0) {
			status = LP_PRINT_HOT;
			break;
		}
	}

	// A print disturbs more than the IO expansion: LoopyManiac saw it stop
	// a timer, and re-arming the scan with bios_vdpMode() reprograms the
	// VDP. Put all of it back.
	LP_ClockRearm();
	LP_InputRescan();
	LP_VideoRestore();
	LP_SoundResume();
	return status;
}
