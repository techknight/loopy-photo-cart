// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Golden tests: the TypeScript ROM patcher must match cart/tools/lpcpack.py
// (whose checksum is itself cross-checked against cart/tools/fixrom.py) byte
// for byte. Regenerate the fixtures with `python cart/tools/mkgolden.py`.

import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

import { loopyChecksum } from "../src/core/checksum.ts";
import { crc32 } from "../src/core/crc32.ts";
import { readDescriptor, TemplateError } from "../src/core/descriptor.ts";
import { buildRom, byteswapRom, PackFormatError, PackTooLargeError } from "../src/core/rompatch.ts";

function fixture(name: string): Uint8Array {
  return new Uint8Array(readFileSync(new URL(`./fixtures/${name}`, import.meta.url)));
}

const template = fixture("template.bin");
const pack = fixture("pack.bin");
const rom = fixture("rom.bin");
const golden = JSON.parse(readFileSync(new URL("./fixtures/golden.json", import.meta.url), "utf8"));

function u32(bytes: Uint8Array, offset: number): number {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(offset);
}

describe("template descriptor", () => {
  it("reads the fields the cart build stamps", () => {
    const d = readDescriptor(template);
    expect(d.descriptorVersion).toBe(1);
    expect(d.packVersion).toBe(1);
    expect(d.packBase).toBe(0x0e040000);
    expect(d.romLimit).toBe(0x0e400000);
    expect(d.buildId).toBe(golden.templateBuildId);
  });

  it("rejects a file without the LPCT magic", () => {
    const bad = template.slice();
    bad[0x20] = 0;
    expect(() => readDescriptor(bad)).toThrow(TemplateError);
  });
});

describe("checksum", () => {
  it("agrees with the value fixrom.py wrote into the template", () => {
    expect(loopyChecksum(template)).toBe(u32(template, 8));
  });
});

describe("crc32", () => {
  it("matches zlib over the pack's tables", () => {
    expect(crc32(pack.subarray(16, golden.tablesEnd))).toBe(golden.headerCrc32);
    expect(u32(pack, 12)).toBe(golden.headerCrc32);
  });

  it("matches the standard check value", () => {
    expect(crc32(new TextEncoder().encode("123456789"))).toBe(0xcbf43926);
  });
});

describe("buildRom", () => {
  it("is byte-identical to the Python reference", () => {
    const out = buildRom(template, pack);
    expect(out.byteLength).toBe(golden.romSize);
    expect(u32(out, 4)).toBe(golden.romLast);
    expect(u32(out, 8)).toBe(golden.checksum);
    expect(Buffer.from(out).equals(Buffer.from(rom))).toBe(true);
  });

  it("does not modify its inputs", () => {
    const t = template.slice();
    const p = pack.slice();
    buildRom(t, p);
    expect(Buffer.from(t).equals(Buffer.from(template))).toBe(true);
    expect(Buffer.from(p).equals(Buffer.from(pack))).toBe(true);
  });

  it("refuses a pack of the wrong version", () => {
    const p = pack.slice();
    p[5] = 2;
    expect(() => buildRom(template, p)).toThrow(TemplateError);
  });

  it("refuses something that is not a pack", () => {
    const p = pack.slice();
    p[0] = 0x58;
    expect(() => buildRom(template, p)).toThrow(PackFormatError);
  });

  it("refuses a template that overlaps the pack region", () => {
    const big = new Uint8Array(0x40000 + 4).fill(0xff);
    big.set(template);
    expect(() => buildRom(big, pack)).toThrow(TemplateError);
  });

  it("accepts a ROM of exactly 4 MB", () => {
    const exact = new Uint8Array(0x3c0000);
    exact.set(pack.subarray(0, 8));
    new DataView(exact.buffer).setUint32(8, exact.byteLength);
    expect(buildRom(template, exact).byteLength).toBe(0x400000);
  });

  it("refuses a ROM larger than the cartridge", () => {
    const huge = new Uint8Array(0x3c0002);
    huge.set(pack.subarray(0, 8));
    new DataView(huge.buffer).setUint32(8, huge.byteLength);
    expect(() => buildRom(template, huge)).toThrow(PackTooLargeError);
  });
});

describe("byteswapRom", () => {
  it("swaps the bytes of every word, and swapping twice gives the ROM back", () => {
    const swapped = byteswapRom(rom);
    expect(swapped.byteLength).toBe(rom.byteLength);
    expect([...swapped.subarray(0, 4)]).toEqual([rom[1], rom[0], rom[3], rom[2]]);
    expect(Buffer.from(byteswapRom(swapped)).equals(Buffer.from(rom))).toBe(true);
  });

  it("refuses an odd length", () => {
    expect(() => byteswapRom(new Uint8Array(3))).toThrow(PackFormatError);
  });
});
