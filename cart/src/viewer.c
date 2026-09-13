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
// A photo change copies the image into the framebuffer and stages its
// palette; the video backend then blits it into the hidden page and flips
// and publishes the palette in the same blanking window, so the new picture
// never shows with the old colours.
//
// On every change a camcorder-style "3/25" counter appears in the top right
// for OSD_FRAMES frames. The pixels under it are saved first and put back
// when it goes, so hiding it costs a small copy rather than a redraw.
//

#include <string.h>

#include "loopy.h"

#include "lp_input.h"
#include "lp_sound.h"
#include "lp_video.h"

#include "help.h"
#include "lpc.h"
#include "pack.h"
#include "printui.h"
#include "text.h"
#include "ui.h"
#include "viewer.h"

// How long the counter stays up. Override for screenshots, e.g.
// `build.ps1 EXTRA_CFLAGS=-DLPC_OSD_FRAMES=100000` keeps it on screen.
#ifdef LPC_OSD_FRAMES
#define OSD_FRAMES LPC_OSD_FRAMES
#else
#define OSD_FRAMES 90
#endif
#define OSD_PAD    3
#define OSD_TOP    16
#define OSD_RIGHT  (LP_FB_W - 16)
#define OSD_H      (LPC_LARGE_HEIGHT + 2 * OSD_PAD)
// "54/54" is the widest counter.
#define OSD_MAX_W  (5 * LPC_LARGE_ADVANCE + 2 * OSD_PAD)

static uint8_t osd_under[OSD_MAX_W * OSD_H];
static int osd_x, osd_w;
static int osd_frames;

// Photos are stored at the printer's resolution (256x224 dots, each dot wider
// than tall), and the TV's pixels are square, so the viewer resamples to show
// the photo in the shape it prints (lpc.h): a landscape photo 256x200 with
// black bars above and below, an upright portrait one 187x240 with bars at
// the sides. Nearest-neighbour, from tables built once.
#define LANDSCAPE_Y ((LP_FB_H - LPC_TV_LANDSCAPE_H) / 2)
#define PORTRAIT_X  ((LP_FB_W - LPC_TV_PORTRAIT_W) / 2)

static uint8_t landscape_row[LPC_TV_LANDSCAPE_H]; // screen row -> stored row
static uint8_t portrait_row[LPC_TV_PORTRAIT_W];   // screen column -> stored row
static uint8_t portrait_col[LP_FB_H];             // screen row -> stored column
static int tables_ready;

static void BuildTables(void)
{
	int i;

	for (i = 0; i < LPC_TV_LANDSCAPE_H; ++i)
		landscape_row[i] = (uint8_t) (i * LPC_PHOTO_H / LPC_TV_LANDSCAPE_H);
	// A portrait photo is stored rotated 90 degrees clockwise: stored pixel
	// (x, y) is upright pixel (y, 255 - x), so the upright picture is 224
	// stored rows wide and 256 stored columns tall.
	for (i = 0; i < LPC_TV_PORTRAIT_W; ++i)
		portrait_row[i] = (uint8_t) (i * LPC_PHOTO_H / LPC_TV_PORTRAIT_W);
	for (i = 0; i < LP_FB_H; ++i)
		portrait_col[i] = (uint8_t) (LPC_IMAGE_W - 1 - i * LPC_IMAGE_W / LP_FB_H);
	tables_ready = 1;
}

static void DrawLandscape(const uint8_t *stored)
{
	uint8_t *fb = LP_Fb();
	int sy;

	memset(fb, LPC_UI_BLACK, (size_t) LP_FB_W * LP_FB_H);
	for (sy = 0; sy < LPC_TV_LANDSCAPE_H; ++sy)
		memcpy(fb + (LANDSCAPE_Y + sy) * LP_FB_W,
		       stored + landscape_row[sy] * LPC_IMAGE_W, LPC_IMAGE_W);
}

static void DrawPortrait(const uint8_t *stored)
{
	uint8_t *fb = LP_Fb();
	int sx, sy;

	memset(fb, LPC_UI_BLACK, (size_t) LP_FB_W * LP_FB_H);
	for (sy = 0; sy < LP_FB_H; ++sy) {
		unsigned col = portrait_col[sy];
		uint8_t *d = fb + sy * LP_FB_W + PORTRAIT_X;

		for (sx = 0; sx < LPC_TV_PORTRAIT_W; ++sx)
			d[sx] = stored[portrait_row[sx] * LPC_IMAGE_W + col];
	}
}

static void ShowPhoto(unsigned index)
{
	struct lpc_photo p;

	if (!tables_ready)
		BuildTables();
	LPC_PackPhoto(index, &p);
	if (p.orientation == LPC_ORIENT_PORTRAIT)
		DrawPortrait(p.pixels);
	else
		DrawLandscape(p.pixels);
	LP_PalLoad(p.palette);
	LP_FbChanged();
	osd_frames = 0;
}

static char *PutUnsigned(char *s, unsigned v)
{
	if (v >= 10)
		*s++ = (char) ('0' + v / 10);
	*s++ = (char) ('0' + v % 10);
	return s;
}

static void OsdShow(unsigned index, unsigned count)
{
	uint8_t *fb = LP_Fb();
	char text[8], *s = text;
	int row;

	s = PutUnsigned(s, index + 1);
	*s++ = '/';
	s = PutUnsigned(s, count);
	*s = '\0';

	osd_w = LPC_TextWidth(LPC_FONT_LARGE, text) + 2 * OSD_PAD;
	osd_x = OSD_RIGHT - osd_w;
	for (row = 0; row < OSD_H; ++row)
		memcpy(osd_under + row * OSD_MAX_W,
		       fb + (OSD_TOP + row) * LP_FB_W + osd_x, (size_t) osd_w);

	LPC_FillRect(osd_x, OSD_TOP, osd_w, OSD_H, LPC_UI_BLACK);
	LPC_TextDraw(LPC_FONT_LARGE, osd_x + OSD_PAD, OSD_TOP + OSD_PAD, text,
	             LPC_UI_WHITE);
	osd_frames = OSD_FRAMES;
}

static void OsdHide(void)
{
	uint8_t *fb = LP_Fb();
	int row;

	for (row = 0; row < OSD_H; ++row)
		memcpy(fb + (OSD_TOP + row) * LP_FB_W + osd_x,
		       osd_under + row * OSD_MAX_W, (size_t) osd_w);
	LP_FbChanged();
}

unsigned LPC_ViewerRun(unsigned index, int can_return)
{
	unsigned count = LPC_PackPhotoCount();

	ShowPhoto(index);
	OsdShow(index, count);

	for (;;) {
		uint16_t edges = LP_PadEdges();

		if (can_return && (edges & GAMEPAD_BTN_B)) {
			LP_SfxPlay(LP_SFX_BUTTON);
			return index;
		} else if (edges & (GAMEPAD_BTN_A | GAMEPAD_BTN_START)) {
			LP_SfxPlay(LP_SFX_BUTTON);
			LPC_PrintPhoto(index, 1);
			ShowPhoto(index);
		} else if (edges & GAMEPAD_BTN_D) {
			LP_SfxPlay(LP_SFX_BUTTON);
			LPC_HelpShow();
			ShowPhoto(index);
		} else if (edges & GAMEPAD_BTN_C) {
			LP_SfxPlay(LP_SFX_BUTTON);
			LP_MusicToggle();
		} else if (edges & (GAMEPAD_BTN_LEFT | GAMEPAD_BTN_LTRIG)) {
			// L too: it steps one photo, like the d-pad.
			LP_SfxPlay(LP_SFX_MOVE);
			index = index ? index - 1 : count - 1;
			ShowPhoto(index);
			OsdShow(index, count);
		} else if (edges & (GAMEPAD_BTN_RIGHT | GAMEPAD_BTN_RTRIG)) {
			LP_SfxPlay(LP_SFX_MOVE);
			index = index + 1 < count ? index + 1 : 0;
			ShowPhoto(index);
			OsdShow(index, count);
		} else if (osd_frames && --osd_frames == 0) {
			OsdHide();
		}

		LP_VideoPresent();
	}
}
