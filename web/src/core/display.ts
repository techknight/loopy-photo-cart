// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Turn indexed images into RGBA for previews, the way the Loopy shows them.

import type { PackImage } from "./pack.ts";
import {
  DOT_H_MM,
  DOT_W_MM,
  expand5,
  IMAGE_W,
  ORIENT_PORTRAIT,
  PAGE_H,
  PHOTO_H,
  TV_LANDSCAPE_H,
  TV_PORTRAIT_W,
  UI_BLACK,
} from "./palette.ts";

export interface RgbaImage {
  width: number;
  height: number;
  data: Uint8ClampedArray;
}

function colour(palette: ArrayLike<number>, index: number, out: Uint8ClampedArray, o: number): void {
  const c = palette[index]!;
  out[o] = expand5((c >> 10) & 31);
  out[o + 1] = expand5((c >> 5) & 31);
  out[o + 2] = expand5(c & 31);
  out[o + 3] = 255;
}

/** An indexed image as-is, `width` x `height`. */
export function indexedRgba(img: PackImage, width: number, height: number): RgbaImage {
  const data = new Uint8ClampedArray(width * height * 4);
  for (let i = 0; i < width * height; i++) colour(img.palette, img.pixels[i]!, data, i * 4);
  return { width, height, data };
}

/** A grid page: a whole 256x240 screen. */
export function pageRgba(img: PackImage): RgbaImage {
  return indexedRgba(img, IMAGE_W, PAGE_H);
}

/** The TV view, exactly as cart/src/viewer.c draws it (256x240, nearest
 *  neighbour from the same tables): a landscape photo 256x200 letterboxed, a
 *  portrait one turned upright at 187x240 pillarboxed. */
export function tvRgba(img: PackImage, orientation: number): RgbaImage {
  const data = new Uint8ClampedArray(IMAGE_W * PAGE_H * 4);
  for (let i = 0; i < IMAGE_W * PAGE_H; i++) colour(img.palette, UI_BLACK, data, i * 4);

  if (orientation !== ORIENT_PORTRAIT) {
    const y0 = Math.floor((PAGE_H - TV_LANDSCAPE_H) / 2);
    for (let sy = 0; sy < TV_LANDSCAPE_H; sy++) {
      const row = Math.floor((sy * PHOTO_H) / TV_LANDSCAPE_H);
      for (let x = 0; x < IMAGE_W; x++) {
        colour(img.palette, img.pixels[row * IMAGE_W + x]!, data, ((y0 + sy) * IMAGE_W + x) * 4);
      }
    }
    return { width: IMAGE_W, height: PAGE_H, data };
  }

  // Stored pixel (x, y) is upright pixel (y, 255 - x).
  const x0 = Math.floor((IMAGE_W - TV_PORTRAIT_W) / 2);
  for (let sy = 0; sy < PAGE_H; sy++) {
    const col = IMAGE_W - 1 - Math.floor((sy * IMAGE_W) / PAGE_H);
    for (let sx = 0; sx < TV_PORTRAIT_W; sx++) {
      const row = Math.floor((sx * PHOTO_H) / TV_PORTRAIT_W);
      colour(img.palette, img.pixels[row * IMAGE_W + col]!, data, (sy * IMAGE_W + x0 + sx) * 4);
    }
  }
  return { width: IMAGE_W, height: PAGE_H, data };
}

/** Width of the sticker preview: the 256 dots stretched to the printed shape
 *  (a dot is wider than it is tall). */
export const STICKER_PREVIEW_W = Math.round((IMAGE_W * DOT_W_MM) / DOT_H_MM);

/** The sticker as it prints: the stored 256x224 photo (a portrait one still
 *  rotated, as the printer receives it) stretched across to the printed
 *  shape, nearest neighbour. */
export function stickerRgba(img: PackImage): RgbaImage {
  const width = STICKER_PREVIEW_W;
  const data = new Uint8ClampedArray(width * PHOTO_H * 4);
  for (let y = 0; y < PHOTO_H; y++) {
    for (let x = 0; x < width; x++) {
      const sx = Math.min(IMAGE_W - 1, Math.floor((x * IMAGE_W) / width));
      colour(img.palette, img.pixels[y * IMAGE_W + sx]!, data, (y * width + x) * 4);
    }
  }
  return { width, height: PHOTO_H, data };
}
