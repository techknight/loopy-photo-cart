// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The previews must show what the cartridge shows (cart/src/viewer.c) and what
// the printer prints.

import { describe, expect, it } from "vitest";

import { STICKER_PREVIEW_W, stickerRgba, tvRgba } from "../src/core/display.ts";
import type { PackImage } from "../src/core/pack.ts";
import { expand5, UI_FIRST, UI_PALETTE } from "../src/core/palette.ts";

// Each stored row (landscape test) or column (portrait test) gets its own
// palette index, so a preview pixel says which stored pixel it came from.
function photo(byRow: boolean): PackImage {
  const palette = [0];
  for (let i = 1; i < UI_FIRST; i++) palette.push(i);
  const pixels = new Uint8Array(256 * 224);
  for (let y = 0; y < 224; y++) {
    for (let x = 0; x < 256; x++) pixels[y * 256 + x] = byRow ? (y % 247) + 1 : (x % 247) + 1;
  }
  return { palette: palette.concat(UI_PALETTE), pixels };
}

// Palette entry i is the RGB555 value i, so blue carries its low 5 bits and
// green the next 5.
function index(rgba: Uint8ClampedArray, o: number): number {
  const g = rgba[o + 1]! >> 3;
  const b = rgba[o + 2]! >> 3;
  return (g << 5) | b;
}

describe("tvRgba", () => {
  it("letterboxes a landscape photo to 256x200 with viewer.c's row table", () => {
    const tv = tvRgba(photo(true), 0);
    expect(tv).toMatchObject({ width: 256, height: 240 });
    expect(tv.data[(10 * 256 + 100) * 4 + 2]).toBe(expand5(0)); // bar is black
    expect(index(tv.data, (20 * 256) * 4)).toBe(1); // screen row 20 <- stored row 0
    expect(index(tv.data, (219 * 256) * 4)).toBe(Math.floor((199 * 224) / 200) + 1);
    expect(tv.data[(225 * 256 + 100) * 4 + 2]).toBe(expand5(0));
  });

  it("turns a portrait photo upright at 187x240", () => {
    const tv = tvRgba(photo(false), 1);
    // screen row 0 reads stored column 255, row 239 stored column 255 - 254.
    expect(index(tv.data, (0 * 256 + 34) * 4)).toBe((255 % 247) + 1);
    expect(index(tv.data, (239 * 256 + 34) * 4)).toBe(((255 - Math.floor((239 * 256) / 240)) % 247) + 1);
    expect(tv.data[(100 * 256 + 20) * 4 + 2]).toBe(0); // left bar
    expect(tv.data[(100 * 256 + 230) * 4 + 2]).toBe(0); // right bar
  });
});

describe("stickerRgba", () => {
  it("stretches the 256x224 photo to the printed shape", () => {
    const s = stickerRgba(photo(false));
    expect(STICKER_PREVIEW_W).toBe(287);
    expect(s).toMatchObject({ width: 287, height: 224 });
    expect(index(s.data, (0 * 287 + 286) * 4)).toBe((255 % 247) + 1);
  });
});
