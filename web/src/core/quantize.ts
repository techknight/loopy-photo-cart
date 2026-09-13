// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Colour reduction to the Loopy's palette rules: a deterministic median cut
// over the RGB555 lattice picks at most 247 colours for slots 1..247, slot 0
// is black and 248..255 carry the UI colours. Pixels map only to slots
// 1..247 -- were the UI slots candidates, error diffusion would reach for
// them and speckle the photo.

import type { RgbImage } from "./image.ts";
import { expand5, PHOTO_COLOURS, UI_FIRST, UI_PALETTE } from "./palette.ts";

export interface IndexedImage {
  width: number;
  height: number;
  /** 256 RGB555 entries. */
  palette: Uint16Array;
  pixels: Uint8Array;
}

function to5(v: number): number {
  const c = Math.min(255, Math.max(0, Math.round(v)));
  return Math.floor((c * 31 + 127) / 255);
}

interface Box {
  keys: number[];
  count: number;
}

function channel(key: number, axis: number): number {
  return (key >> (10 - axis * 5)) & 31;
}

function boxRange(box: Box, hist: Uint32Array): { axis: number; range: number } {
  let best = { axis: 0, range: -1 };
  for (let axis = 0; axis < 3; axis++) {
    let lo = 31;
    let hi = 0;
    for (const k of box.keys) {
      const v = channel(k, axis);
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    if (hi - lo > best.range) best = { axis, range: hi - lo };
  }
  void hist;
  return best;
}

function medianCut(hist: Uint32Array, maxColours: number): number[] {
  const keys: number[] = [];
  let total = 0;
  for (let k = 0; k < hist.length; k++) {
    if (hist[k]! > 0) {
      keys.push(k);
      total += hist[k]!;
    }
  }
  if (keys.length === 0) return [];

  const boxes: Box[] = [{ keys, count: total }];
  while (boxes.length < maxColours) {
    // Split the box with the most pixels among those that can be split,
    // weighted by its widest channel range; ties go to the earliest box.
    let pick = -1;
    let pickScore = -1;
    let pickAxis = 0;
    for (let i = 0; i < boxes.length; i++) {
      const b = boxes[i]!;
      if (b.keys.length < 2) continue;
      const { axis, range } = boxRange(b, hist);
      const score = b.count * (range + 1);
      if (score > pickScore) {
        pick = i;
        pickScore = score;
        pickAxis = axis;
      }
    }
    if (pick < 0) break;

    const box = boxes[pick]!;
    box.keys.sort((a, b) => channel(a, pickAxis) - channel(b, pickAxis) || a - b);
    let acc = 0;
    let cut = 0;
    while (cut < box.keys.length - 1) {
      acc += hist[box.keys[cut]!]!;
      cut++;
      if (acc * 2 >= box.count) break;
    }
    const left = box.keys.slice(0, cut);
    const right = box.keys.slice(cut);
    const leftCount = left.reduce((s, k) => s + hist[k]!, 0);
    boxes.splice(pick, 1, { keys: left, count: leftCount }, { keys: right, count: box.count - leftCount });
  }

  return boxes.map((b) => {
    let r = 0;
    let g = 0;
    let bl = 0;
    for (const k of b.keys) {
      const n = hist[k]!;
      r += channel(k, 0) * n;
      g += channel(k, 1) * n;
      bl += channel(k, 2) * n;
    }
    return (
      (Math.round(r / b.count) << 10) | (Math.round(g / b.count) << 5) | Math.round(bl / b.count)
    );
  });
}

/** Quantize a 256x240-ish RGB image. */
export function quantize(img: RgbImage, dither: boolean): IndexedImage {
  const n = img.width * img.height;
  const src = img.data;
  const hist = new Uint32Array(32768);
  const keys = new Uint16Array(n);
  for (let i = 0; i < n; i++) {
    const k = (to5(src[i * 3]!) << 10) | (to5(src[i * 3 + 1]!) << 5) | to5(src[i * 3 + 2]!);
    keys[i] = k;
    hist[k]!++;
  }

  const colours = medianCut(hist, PHOTO_COLOURS);
  const palette = new Uint16Array(256);
  colours.forEach((c, i) => (palette[i + 1] = c));
  UI_PALETTE.forEach((c, i) => (palette[UI_FIRST + i] = c));

  const slots = colours.length;
  const pr = new Float32Array(slots);
  const pg = new Float32Array(slots);
  const pb = new Float32Array(slots);
  colours.forEach((c, i) => {
    pr[i] = expand5((c >> 10) & 31);
    pg[i] = expand5((c >> 5) & 31);
    pb[i] = expand5(c & 31);
  });

  const cache = new Int16Array(32768).fill(-1);
  const nearest = (key: number): number => {
    const hit = cache[key]!;
    if (hit >= 0) return hit;
    const r = expand5((key >> 10) & 31);
    const g = expand5((key >> 5) & 31);
    const b = expand5(key & 31);
    let best = 0;
    let bestD = Infinity;
    for (let i = 0; i < slots; i++) {
      const dr = r - pr[i]!;
      const dg = g - pg[i]!;
      const db = b - pb[i]!;
      const d = dr * dr + dg * dg + db * db;
      if (d < bestD) {
        bestD = d;
        best = i;
      }
    }
    cache[key] = best + 1;
    return best + 1;
  };

  const pixels = new Uint8Array(n);
  if (slots === 0) {
    return { width: img.width, height: img.height, palette, pixels };
  }

  if (!dither) {
    for (let i = 0; i < n; i++) pixels[i] = nearest(keys[i]!);
  } else {
    // Floyd-Steinberg in 8-bit space, left to right.
    const w = img.width;
    const work = new Float32Array(src);
    for (let y = 0; y < img.height; y++) {
      for (let x = 0; x < w; x++) {
        const i = y * w + x;
        const o = i * 3;
        const r = Math.min(255, Math.max(0, work[o]!));
        const g = Math.min(255, Math.max(0, work[o + 1]!));
        const b = Math.min(255, Math.max(0, work[o + 2]!));
        const slot = nearest((to5(r) << 10) | (to5(g) << 5) | to5(b));
        pixels[i] = slot;
        const er = r - pr[slot - 1]!;
        const eg = g - pg[slot - 1]!;
        const eb = b - pb[slot - 1]!;
        const spread = (dx: number, dy: number, f: number) => {
          const xx = x + dx;
          const yy = y + dy;
          if (xx < 0 || xx >= w || yy >= img.height) return;
          const p = (yy * w + xx) * 3;
          work[p]! += er * f;
          work[p + 1]! += eg * f;
          work[p + 2]! += eb * f;
        };
        spread(1, 0, 7 / 16);
        spread(-1, 1, 3 / 16);
        spread(0, 1, 5 / 16);
        spread(1, 1, 1 / 16);
      }
    }
  }
  return { width: img.width, height: img.height, palette, pixels };
}
