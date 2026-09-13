// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Where to put the starting crop. Scores detail (local contrast) and
// saturation on a small copy of the photo, then slides the largest
// sticker-shaped box along the photo's long axis and keeps the position with
// the most interest, gently preferring the centre.

import type { PixelSource } from "./image.ts";
import { coverSize } from "./crop.ts";
import { resize } from "./resample.ts";

const ANALYSIS_W = 128;

export function suggestFocus(src: PixelSource, aspect: number): { x: number; y: number } {
  const scale = Math.min(1, ANALYSIS_W / src.width);
  const w = Math.max(8, Math.round(src.width * scale));
  const h = Math.max(8, Math.round(src.height * scale));
  const small = resize(src, w, h);
  const d = small.data;

  const luma = new Float32Array(w * h);
  for (let i = 0; i < w * h; i++) luma[i] = 0.299 * d[i * 3]! + 0.587 * d[i * 3 + 1]! + 0.114 * d[i * 3 + 2]!;

  // Integral image of the interest score.
  const integral = new Float64Array((w + 1) * (h + 1));
  for (let y = 0; y < h; y++) {
    let row = 0;
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      const l = luma[i]!;
      const n = (luma[Math.max(0, y - 1) * w + x]! + luma[Math.min(h - 1, y + 1) * w + x]! +
                 luma[y * w + Math.max(0, x - 1)]! + luma[y * w + Math.min(w - 1, x + 1)]!) / 4;
      const detail = Math.abs(l - n);
      const r = d[i * 3]!, g = d[i * 3 + 1]!, b = d[i * 3 + 2]!;
      const sat = Math.max(r, g, b) - Math.min(r, g, b);
      row += detail + sat * 0.15;
      integral[(y + 1) * (w + 1) + x + 1] = integral[y * (w + 1) + x + 1]! + row;
    }
  }
  const sum = (x0: number, y0: number, x1: number, y1: number) =>
    integral[y1 * (w + 1) + x1]! - integral[y0 * (w + 1) + x1]! - integral[y1 * (w + 1) + x0]! + integral[y0 * (w + 1) + x0]!;

  const box = coverSize(w, h, aspect);
  const bw = Math.min(w, Math.round(box.w));
  const bh = Math.min(h, Math.round(box.h));
  let bestX = Math.floor((w - bw) / 2);
  let bestY = Math.floor((h - bh) / 2);
  let best = -Infinity;
  const spanX = w - bw;
  const spanY = h - bh;
  for (let y = 0; y <= spanY; y++) {
    for (let x = 0; x <= spanX; x++) {
      const off = (spanX ? (x / spanX - 0.5) ** 2 : 0) + (spanY ? (y / spanY - 0.5) ** 2 : 0);
      const score = sum(x, y, x + bw, y + bh) * (1 - 0.35 * off);
      if (score > best) {
        best = score;
        bestX = x;
        bestY = y;
      }
    }
  }
  return { x: (bestX + bw / 2) / scale, y: (bestY + bh / 2) / scale };
}
