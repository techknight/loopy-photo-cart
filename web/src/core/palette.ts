// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Image dimensions and the reserved palette slots (docs/pack-format.md).

export const IMAGE_W = 256;
export const IMAGE_H = 240;
export const IMAGE_PIXELS = IMAGE_W * IMAGE_H;

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
