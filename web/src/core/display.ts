// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Turn indexed images into RGBA for previews, the way the Loopy shows them.

import type { PackImage } from "./pack.ts";
import { expand5, IMAGE_H, IMAGE_W, ORIENT_PORTRAIT, UI_BLACK } from "./palette.ts";

function colour(palette: ArrayLike<number>, index: number, out: Uint8ClampedArray, o: number): void {
  const c = palette[index]!;
  out[o] = expand5((c >> 10) & 31);
  out[o + 1] = expand5((c >> 5) & 31);
  out[o + 2] = expand5(c & 31);
  out[o + 3] = 255;
}

/** The stored 256x240 buffer as-is: what the printer receives. */
export function storedRgba(img: PackImage): Uint8ClampedArray {
  const out = new Uint8ClampedArray(IMAGE_W * IMAGE_H * 4);
  for (let i = 0; i < IMAGE_W * IMAGE_H; i++) colour(img.palette, img.pixels[i]!, out, i * 4);
  return out;
}

const PORTRAIT_W = 225;

/** The TV view (cart/src/viewer.c): portrait photos turned upright and
 *  scaled to 240 lines, with black bars. */
export function tvRgba(img: PackImage, orientation: number): Uint8ClampedArray {
  if (orientation !== ORIENT_PORTRAIT) return storedRgba(img);
  const out = new Uint8ClampedArray(IMAGE_W * IMAGE_H * 4);
  const x0 = Math.floor((IMAGE_W - PORTRAIT_W) / 2);
  for (let i = 0; i < IMAGE_W * IMAGE_H; i++) colour(img.palette, UI_BLACK, out, i * 4);
  for (let sy = 0; sy < IMAGE_H; sy++) {
    const col = IMAGE_W - 1 - Math.floor((sy * IMAGE_W) / IMAGE_H);
    for (let sx = 0; sx < PORTRAIT_W; sx++) {
      const row = Math.floor((sx * IMAGE_H) / PORTRAIT_W);
      colour(img.palette, img.pixels[row * IMAGE_W + col]!, out, (sy * IMAGE_W + x0 + sx) * 4);
    }
  }
  return out;
}
