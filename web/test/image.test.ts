// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later

import { describe, expect, it } from "vitest";

import {
  clampCrop,
  cropRect,
  defaultCrop,
  keptFraction,
  renderUpright,
  STICKER_ASPECT,
  thumbnailOf,
  type CropState,
} from "../src/core/crop.ts";
import { newRgb, rotateClockwise } from "../src/core/image.ts";
import { UI_FIRST, UI_PALETTE } from "../src/core/palette.ts";
import { quantize } from "../src/core/quantize.ts";
import { resize } from "../src/core/resample.ts";
import { suggestFocus } from "../src/core/saliency.ts";

function gradient(w: number, h: number) {
  const img = newRgb(w, h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 3;
      img.data[o] = (x * 255) / (w - 1);
      img.data[o + 1] = (y * 255) / (h - 1);
      img.data[o + 2] = ((x + y) * 127) / (w + h);
    }
  }
  return img;
}

describe("crop", () => {
  it("fills the sticker shape and stays inside the photo", () => {
    const s: CropState = { orientation: "landscape", mode: "fill", zoom: 2, cx: 0, cy: 10_000 };
    const r = cropRect(1600, 1200, s);
    expect(r.w / r.h).toBeCloseTo(STICKER_ASPECT);
    expect(r.x).toBe(0);
    expect(r.y + r.h).toBeCloseTo(1200);
    // Zoom 2 keeps a quarter of the largest sticker-shaped box, which is the
    // full height of a 4:3 photo and STICKER_ASPECT times as wide.
    expect(keptFraction(1600, 1200, s)).toBeCloseTo((1200 * STICKER_ASPECT * 1200) / 4 / (1600 * 1200));
  });

  it("starts tall photos as portrait stickers", () => {
    const c = defaultCrop(1200, 1600);
    expect(c.orientation).toBe("portrait");
    const r = cropRect(1200, 1600, c);
    expect(r.w / r.h).toBeCloseTo(1 / STICKER_ASPECT);
  });

  it("clamps zoom and centre", () => {
    const c = clampCrop(800, 600, { orientation: "landscape", mode: "fill", zoom: 99, cx: -5, cy: -5 });
    expect(c.zoom).toBe(6);
    const r = cropRect(800, 600, c);
    expect(r.x).toBe(0);
    expect(r.y).toBe(0);
  });

  it("uses the measured printed shape", () => {
    expect(STICKER_ASPECT).toBeCloseTo((256 * 0.16) / (224 * 0.1425), 10);
    expect(STICKER_ASPECT).toBeCloseTo(1.2835, 3);
  });

  it("renders in printer dots: portrait upright at 224x256, fit mode at 256x224", () => {
    const src = gradient(300, 400);
    expect(renderUpright(src, defaultCrop(300, 400))).toMatchObject({ width: 224, height: 256 });
    const fit = renderUpright(src, { ...defaultCrop(300, 400), orientation: "landscape", mode: "fit" });
    expect(fit).toMatchObject({ width: 256, height: 224 });
  });

  it("cuts thumbnails from the crop at square pixels", () => {
    // A photo whose left half is black and right half white: a centred
    // thumbnail of a full-width crop must be half and half.
    const src = newRgb(1284, 1000);
    for (let y = 0; y < 1000; y++) src.data.fill(255, (y * 1284 + 642) * 3, (y * 1284 + 1284) * 3);
    const t = thumbnailOf(src, defaultCrop(1284, 1000), 64, 60);
    expect(t).toMatchObject({ width: 64, height: 60 });
    expect(t.data[(30 * 64 + 10) * 3]).toBeLessThan(10);
    expect(t.data[(30 * 64 + 54) * 3]).toBeGreaterThan(245);
  });

  it("finds the interesting part of a photo", () => {
    // A flat grey photo with a busy patch near the right edge.
    const img = newRgb(400, 200, 128);
    for (let y = 60; y < 140; y++) {
      for (let x = 320; x < 390; x++) {
        const v = (x + y) % 2 ? 255 : 0;
        img.data.fill(v, (y * 400 + x) * 3, (y * 400 + x) * 3 + 3);
      }
    }
    const f = suggestFocus(img, STICKER_ASPECT);
    expect(f.x).toBeGreaterThan(200);
  });
});

describe("rotateClockwise", () => {
  it("maps stored (x, y) to upright (y, 255 - x)", () => {
    const up = newRgb(224, 256);
    up.data[(5 * 224 + 7) * 3] = 99; // upright (7, 5)
    const stored = rotateClockwise(up);
    expect(stored).toMatchObject({ width: 256, height: 224 });
    // upright (ux = 7, uy = 5) is stored (x = 255 - 5, y = 7)
    expect(stored.data[(7 * 256 + 250) * 3]).toBe(99);
  });
});

describe("quantize", () => {
  it("obeys the palette slot rules and is deterministic", () => {
    const img = resize(gradient(640, 480), 256, 240);
    for (const dither of [false, true]) {
      const a = quantize(img, dither);
      const b = quantize(img, dither);
      expect(Buffer.from(a.pixels).equals(Buffer.from(b.pixels))).toBe(true);
      expect(a.palette[0]).toBe(0);
      expect(Array.from(a.palette.subarray(UI_FIRST))).toEqual(UI_PALETTE);
      for (const p of a.pixels) {
        expect(p).toBeGreaterThanOrEqual(1);
        expect(p).toBeLessThan(UI_FIRST);
      }
    }
  });

  it("reproduces a few flat colours exactly", () => {
    const img = newRgb(256, 240);
    for (let i = 0; i < 256 * 240; i++) {
      const [r, g, b] = i % 3 === 0 ? [255, 0, 0] : i % 3 === 1 ? [0, 0, 255] : [255, 255, 255];
      img.data.set([r, g, b], i * 3);
    }
    const q = quantize(img, true);
    const used = new Set(q.pixels);
    expect(used.size).toBe(3);
    const colours = [...used].map((i) => q.palette[i]).sort();
    expect(colours).toEqual([0x001f, 0x7c00, 0x7fff].sort());
  });
});
