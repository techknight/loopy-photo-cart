// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Pixel containers shared by the pipeline. No DOM types: the same code runs
// in the browser's worker and under Node.

/** Interleaved pixels, 3 (RGB) or 4 (RGBA; alpha ignored) channels, 0..255. */
export interface PixelSource {
  width: number;
  height: number;
  channels: 3 | 4;
  data: ArrayLike<number>;
}

/** Working RGB image: 3 float channels per pixel, nominally 0..255. */
export interface RgbImage {
  width: number;
  height: number;
  channels: 3;
  data: Float32Array;
}

export function newRgb(width: number, height: number, fill = 0): RgbImage {
  const data = new Float32Array(width * height * 3);
  if (fill !== 0) data.fill(fill);
  return { width, height, channels: 3, data };
}

/** Copy `src` into `dst` with its top-left at (x, y), clipped. */
export function paste(dst: RgbImage, src: RgbImage, x: number, y: number): void {
  for (let sy = 0; sy < src.height; sy++) {
    const dy = y + sy;
    if (dy < 0 || dy >= dst.height) continue;
    for (let sx = 0; sx < src.width; sx++) {
      const dx = x + sx;
      if (dx < 0 || dx >= dst.width) continue;
      const s = (sy * src.width + sx) * 3;
      const d = (dy * dst.width + dx) * 3;
      dst.data[d] = src.data[s]!;
      dst.data[d + 1] = src.data[s + 1]!;
      dst.data[d + 2] = src.data[s + 2]!;
    }
  }
}

/** Upright 240x256 portrait -> stored 256x240, rotated 90 degrees clockwise.
 *  Stored pixel (x, y) is upright pixel (y, H - 1 - x). */
export function rotateClockwise(src: RgbImage): RgbImage {
  const out = newRgb(src.height, src.width);
  for (let y = 0; y < out.height; y++) {
    for (let x = 0; x < out.width; x++) {
      const s = ((src.height - 1 - x) * src.width + y) * 3;
      const d = (y * out.width + x) * 3;
      out.data[d] = src.data[s]!;
      out.data[d + 1] = src.data[s + 1]!;
      out.data[d + 2] = src.data[s + 2]!;
    }
  }
  return out;
}
