// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The whole core pipeline on synthetic photos: crop, resize, quantize, grid
// pages, pack, ROM -- and the result must pass the cartridge's validation.

import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

import { defaultCrop } from "../src/core/crop.ts";
import type { PixelSource } from "../src/core/image.ts";
import { PackError } from "../src/core/pack.ts";
import { buildCartridge, renderPhoto } from "../src/core/pipeline.ts";
import { validateRom } from "../src/core/validate.ts";

const template = new Uint8Array(readFileSync(new URL("./fixtures/template.bin", import.meta.url)));

function rgbaPhoto(w: number, h: number, seed: number): PixelSource {
  const data = new Uint8ClampedArray(w * h * 4);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4;
      data[o] = (x * seed) % 256;
      data[o + 1] = (y * 3 + seed * 20) % 256;
      data[o + 2] = ((x ^ y) * seed) % 256;
      data[o + 3] = 255;
    }
  }
  return { width: w, height: h, channels: 4, data };
}

describe("pipeline", () => {
  it("builds a ROM the cartridge accepts", () => {
    const rendered = [
      renderPhoto(rgbaPhoto(640, 480, 3), defaultCrop(640, 480), true),
      renderPhoto(rgbaPhoto(480, 720, 5), defaultCrop(480, 720), true),
      renderPhoto(rgbaPhoto(800, 300, 7), { ...defaultCrop(800, 300), mode: "fit" }, false),
    ];
    expect(rendered[1]!.photo.orientation).toBe(1);

    const rom = buildCartridge(template, rendered, { created: "2026-09-13" });
    const report = validateRom(rom);
    expect(report.errors).toEqual([]);
    expect(report.photos).toBe(3);
    expect(report.pages).toBe(1);
  });

  it("fits 54 photos in 4 MB and refuses a 55th", () => {
    const one = renderPhoto(rgbaPhoto(320, 240, 9), defaultCrop(320, 240), true);
    const rom = buildCartridge(template, Array(54).fill(one));
    expect(rom.length).toBeLessThanOrEqual(0x400000);
    const report = validateRom(rom);
    expect(report.errors).toEqual([]);
    expect(report.pages).toBe(6);
    expect(() => buildCartridge(template, Array(55).fill(one))).toThrow(PackError);
  });

  it("catches a corrupted ROM", () => {
    const one = renderPhoto(rgbaPhoto(320, 240, 9), defaultCrop(320, 240), true);
    const rom = buildCartridge(template, [one]);
    rom[0x40000 + 17] ^= 0xff; // photo count, inside the CRC'd tables
    expect(validateRom(rom).errors.length).toBeGreaterThan(0);
  });
});
