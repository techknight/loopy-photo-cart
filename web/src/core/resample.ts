// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Separable Lanczos-3 resampling of a (sub-)rectangle. When shrinking, the
// kernel widens with the scale, so it integrates over the source like an area
// filter instead of aliasing.

import type { PixelSource, RgbImage } from "./image.ts";
import { newRgb } from "./image.ts";

function lanczos3(x: number): number {
  if (x === 0) return 1;
  const ax = Math.abs(x);
  if (ax >= 3) return 0;
  const px = Math.PI * x;
  return (3 * Math.sin(px) * Math.sin(px / 3)) / (px * px);
}

interface Weights {
  taps: number;
  index: Int32Array;
  weight: Float32Array;
}

function makeWeights(srcLen: number, start: number, size: number, dstLen: number): Weights {
  const scale = size / dstLen;
  const filterScale = Math.max(1, scale);
  const support = 3 * filterScale;
  const taps = Math.floor(2 * support) + 2;
  const index = new Int32Array(dstLen * taps);
  const weight = new Float32Array(dstLen * taps);

  for (let i = 0; i < dstLen; i++) {
    const center = start + (i + 0.5) * scale;
    const left = Math.floor(center - support);
    let sum = 0;
    for (let t = 0; t < taps; t++) {
      const j = left + t;
      const w = lanczos3((j + 0.5 - center) / filterScale);
      index[i * taps + t] = Math.min(srcLen - 1, Math.max(0, j));
      weight[i * taps + t] = w;
      sum += w;
    }
    if (sum !== 0) {
      for (let t = 0; t < taps; t++) weight[i * taps + t]! /= sum;
    }
  }
  return { taps, index, weight };
}

/** Resample the rectangle (sx, sy, sw, sh) of `src` (fractional allowed) to
 *  dw x dh. Samples outside the rectangle but inside the image are used at
 *  its edges; outside the image, edge pixels repeat. */
export function resampleRect(
  src: PixelSource,
  sx: number,
  sy: number,
  sw: number,
  sh: number,
  dw: number,
  dh: number,
): RgbImage {
  const ch = src.channels;
  const hw = makeWeights(src.width, sx, sw, dw);
  const vw = makeWeights(src.height, sy, sh, dh);

  let rowMin = src.height;
  let rowMax = -1;
  for (let i = 0; i < vw.index.length; i++) {
    if (vw.weight[i] === 0) continue;
    rowMin = Math.min(rowMin, vw.index[i]!);
    rowMax = Math.max(rowMax, vw.index[i]!);
  }
  if (rowMax < rowMin) {
    rowMin = 0;
    rowMax = 0;
  }
  const rows = rowMax - rowMin + 1;

  // Horizontal pass over just the rows the vertical pass reads.
  const tmp = new Float32Array(rows * dw * 3);
  const data = src.data;
  for (let r = 0; r < rows; r++) {
    const srcRow = (rowMin + r) * src.width;
    for (let x = 0; x < dw; x++) {
      let cr = 0;
      let cg = 0;
      let cb = 0;
      const base = x * hw.taps;
      for (let t = 0; t < hw.taps; t++) {
        const w = hw.weight[base + t]!;
        if (w === 0) continue;
        const p = (srcRow + hw.index[base + t]!) * ch;
        cr += data[p]! * w;
        cg += data[p + 1]! * w;
        cb += data[p + 2]! * w;
      }
      const o = (r * dw + x) * 3;
      tmp[o] = cr;
      tmp[o + 1] = cg;
      tmp[o + 2] = cb;
    }
  }

  const out = newRgb(dw, dh);
  for (let y = 0; y < dh; y++) {
    const base = y * vw.taps;
    for (let x = 0; x < dw; x++) {
      let cr = 0;
      let cg = 0;
      let cb = 0;
      for (let t = 0; t < vw.taps; t++) {
        const w = vw.weight[base + t]!;
        if (w === 0) continue;
        const p = ((vw.index[base + t]! - rowMin) * dw + x) * 3;
        cr += tmp[p]! * w;
        cg += tmp[p + 1]! * w;
        cb += tmp[p + 2]! * w;
      }
      const o = (y * dw + x) * 3;
      out.data[o] = cr;
      out.data[o + 1] = cg;
      out.data[o + 2] = cb;
    }
  }
  return out;
}

/** Whole-image resize. */
export function resize(src: PixelSource, dw: number, dh: number): RgbImage {
  return resampleRect(src, 0, 0, src.width, src.height, dw, dh);
}

/** Box blur, `passes` times (three approximate a Gaussian). */
export function boxBlur(img: RgbImage, radius: number, passes = 3): RgbImage {
  let cur = img.data;
  const { width: w, height: h } = img;
  const size = radius * 2 + 1;
  for (let p = 0; p < passes; p++) {
    const horiz = new Float32Array(cur.length);
    for (let y = 0; y < h; y++) {
      for (let c = 0; c < 3; c++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          acc += cur[(y * w + Math.min(w - 1, Math.max(0, k))) * 3 + c]!;
        }
        for (let x = 0; x < w; x++) {
          horiz[(y * w + x) * 3 + c] = acc / size;
          const add = Math.min(w - 1, x + radius + 1);
          const sub = Math.max(0, x - radius);
          acc += cur[(y * w + add) * 3 + c]! - cur[(y * w + sub) * 3 + c]!;
        }
      }
    }
    const vert = new Float32Array(cur.length);
    for (let x = 0; x < w; x++) {
      for (let c = 0; c < 3; c++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          acc += horiz[(Math.min(h - 1, Math.max(0, k)) * w + x) * 3 + c]!;
        }
        for (let y = 0; y < h; y++) {
          vert[(y * w + x) * 3 + c] = acc / size;
          const add = Math.min(h - 1, y + radius + 1);
          const sub = Math.max(0, y - radius);
          acc += horiz[(add * w + x) * 3 + c]! - horiz[(sub * w + x) * 3 + c]!;
        }
      }
    }
    cur = vert;
  }
  return { width: w, height: h, channels: 3, data: cur };
}
