// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Image dimensions and the reserved palette slots (docs/pack-format.md).

export const IMAGE_W = 256;
/** A photo is stored at the printer's own resolution: the sticker shows 256
 *  dots by 224 lines (measured on hardware; the rest of the BIOS's 241-line
 *  buffer is cut off). */
export const PHOTO_H = 224;
/** A grid page is a whole screen. */
export const PAGE_H = 240;
export const PHOTO_PIXELS = IMAGE_W * PHOTO_H;
export const PAGE_PIXELS = IMAGE_W * PAGE_H;

/** Printer dot size in mm, measured on an XS-11 sticker (200 dots across were
 *  32 mm, 200 lines down 28.5 mm). Must match cart/tools/lpcpack.py. */
export const DOT_W_MM = 0.16;
export const DOT_H_MM = 0.1425;

/** The TV's pixels are square, so the cart shows a photo in its printed shape
 *  as 256x200 (landscape, letterboxed) or 187x240 (portrait, pillarboxed).
 *  Must match cart/src/lpc.h. */
export const TV_LANDSCAPE_H = 200;
export const TV_PORTRAIT_W = 187;

export const MAX_PHOTOS = 54;
export const CELLS_PER_PAGE = 9;
export const THUMB_W = 64;
export const THUMB_H = 60;
export const CURSOR_RING = 4;

export const ORIENT_LANDSCAPE = 0;
export const ORIENT_PORTRAIT = 1;

/** Slot 0 is black (it doubles as the backdrop); 248..255 are the UI's. */
export const UI_FIRST = 248;
/** Photos use slots 1..247. */
export const PHOTO_COLOURS = UI_FIRST - 1;

export const UI_BLACK = 248;
export const UI_BG = 249;
export const UI_PANEL = 250;
export const UI_DIM = 251;
export const UI_GREY = 252;
export const UI_WHITE = 253;
export const UI_ACCENT = 254;
export const UI_HIGHLIGHT = 255;

/** Slots 248..255 as 5-bit (r, g, b); must match cart/src/ui.c. */
export const UI_COLOURS_5BIT: readonly (readonly [number, number, number])[] = [
  [0, 0, 0],
  [3, 4, 8],
  [6, 7, 13],
  [12, 12, 15],
  [22, 22, 24],
  [31, 31, 31],
  [31, 14, 20],
  [31, 27, 8],
];

export function rgb555(r: number, g: number, b: number): number {
  return (r << 10) | (g << 5) | b;
}

export const UI_PALETTE: readonly number[] = UI_COLOURS_5BIT.map(([r, g, b]) => rgb555(r, g, b));

/** 5-bit channel to 8 bits, the way the display does it. */
export function expand5(v: number): number {
  return (v << 3) | (v >> 2);
}
