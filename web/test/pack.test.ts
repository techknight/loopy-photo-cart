// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The TypeScript pack encoder must match cart/tools/lpcpack.py byte for byte.
// Fixtures: `python cart/tools/mkgolden_web.py`, whose synthetic images this
// file rebuilds from the same formulas.

import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

import { encodePack, PackError, type PackImage } from "../src/core/pack.ts";
import { cellsFor } from "../src/core/pages.ts";
import { rgb555, UI_FIRST, UI_PALETTE } from "../src/core/palette.ts";

const W = 256;
const H = 240;

function palette(seed: number): number[] {
  const p = [0];
  for (let i = 1; i < UI_FIRST; i++) p.push(rgb555((i * seed) & 31, (i >> 3) & 31, ((255 - i) >> 3) & 31));
  return p.concat(UI_PALETTE);
}

function image(seed: number): PackImage {
  const pixels = new Uint8Array(W * H);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) pixels[y * W + x] = ((x * seed + y) % 247) + 1;
  return { palette: palette(seed), pixels };
}

describe("encodePack", () => {
  it("is byte-identical to lpcpack.encode_pack", () => {
    const printPalette = [0];
    for (let i = 1; i < UI_FIRST; i++) printPalette.push(rgb555(31 - (i >> 3), i >> 3, 15));
    const pack = encodePack(
      [
        { image: image(3), orientation: 0 },
        { image: image(5), orientation: 1, printPalette: printPalette.concat(UI_PALETTE) },
        { image: image(11), orientation: 0 },
      ],
      [{ image: image(7), firstPhoto: 0, cells: cellsFor(3) }],
      { title: "Golden", tool: "mkgolden_web", created: "2026-09-13" },
    );
    const golden = new Uint8Array(readFileSync(new URL("./fixtures/pack-pages.bin", import.meta.url)));
    expect(pack.length).toBe(golden.length);
    expect(Buffer.from(pack).equals(Buffer.from(golden))).toBe(true);
  });

  it("refuses a palette whose slot 0 is not black", () => {
    const img = image(3);
    const bad = { palette: [1, ...Array.from(img.palette).slice(1)], pixels: img.pixels };
    expect(() => encodePack([{ image: bad, orientation: 0 }])).toThrow(PackError);
  });

  it("refuses the wrong page count", () => {
    const photos = Array.from({ length: 10 }, () => ({ image: image(3), orientation: 0 }));
    expect(() => encodePack(photos, [{ image: image(7), firstPhoto: 0, cells: cellsFor(9) }])).toThrow(PackError);
  });

  it("refuses 55 photos", () => {
    const photos = Array.from({ length: 55 }, () => ({ image: image(3), orientation: 0 }));
    expect(() => encodePack(photos)).toThrow(PackError);
  });
});
