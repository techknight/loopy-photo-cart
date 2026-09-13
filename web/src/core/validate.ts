// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Check a finished ROM the way the cartridge will (cart/src/pack.c), plus the
// encoder-side palette rules. The web app runs this on every ROM before
// offering it for download; cart/tools/lpcvalidate.py is the Python twin.

import { loopyChecksum } from "./checksum.ts";
import { PACK_MAGIC, PACK_VERSION, ROM_BASE } from "./constants.ts";
import { crc32 } from "./crc32.ts";
import { readDescriptor } from "./descriptor.ts";
import { IMAGE_H, IMAGE_PIXELS, IMAGE_W, MAX_PHOTOS, UI_FIRST, UI_PALETTE } from "./palette.ts";

export interface RomReport {
  errors: string[];
  photos: number;
  pages: number;
  packBytes: number;
}

export function validateRom(rom: Uint8Array): RomReport {
  const errors: string[] = [];
  const report: RomReport = { errors, photos: 0, pages: 0, packBytes: 0 };
  const v = new DataView(rom.buffer, rom.byteOffset, rom.byteLength);

  let d;
  try {
    d = readDescriptor(rom);
  } catch (e) {
    errors.push(String(e));
    return report;
  }
  const romEnd = v.getUint32(4) + 2 - ROM_BASE;
  if (romEnd > rom.length) {
    errors.push("header romlast points past the file");
    return report;
  }
  if (v.getUint32(8) !== loopyChecksum(rom)) errors.push("header checksum is wrong");
  if (ROM_BASE + romEnd > d.romLimit) errors.push("ROM exceeds the cartridge limit");

  const base = d.packBase - ROM_BASE;
  if (romEnd < base + 16) {
    errors.push("no photo pack");
    return report;
  }
  const magic = String.fromCharCode(...rom.subarray(base, base + 4));
  if (magic !== PACK_MAGIC || v.getUint16(base + 4) !== PACK_VERSION) {
    errors.push("bad pack magic or version");
    return report;
  }
  const u16 = (o: number) => v.getUint16(base + o);
  const u32 = (o: number) => v.getUint32(base + o);
  const total = u32(8);
  const tablesEnd = u32(20);
  report.packBytes = total;
  if (total < 40 || base + total > romEnd) {
    errors.push("pack size out of range");
    return report;
  }
  if (tablesEnd % 4 || tablesEnd < 40 || tablesEnd > total) {
    errors.push("bad tables_end");
    return report;
  }
  if (crc32(rom.subarray(base + 16, base + tablesEnd)) !== u32(12)) errors.push("header CRC mismatch");

  const photos = u16(16);
  const pages = u16(18);
  report.photos = photos;
  report.pages = pages;
  if (photos > MAX_PHOTOS) errors.push("too many photos");
  if (pages !== 0 && pages !== Math.ceil(photos / 9)) errors.push("bad page count");
  if (rom[base + 36] !== 3 || rom[base + 37] !== 3) errors.push("grid must be 3x3");

  const span = (off: number, len: number, lo: number, hi: number) =>
    off % 4 === 0 && off >= lo && off <= hi && len <= hi - off;
  const checkPalette = (off: number, what: string) => {
    if (v.getUint16(base + off) !== 0) errors.push(`${what}: palette slot 0 is not black`);
    for (let i = 0; i < 8; i++) {
      if (v.getUint16(base + off + (UI_FIRST + i) * 2) !== UI_PALETTE[i]) {
        errors.push(`${what}: UI slots 248-255 are wrong`);
        break;
      }
    }
  };
  const imageRef = (off: number, what: string, photo: boolean) => {
    const pal = u32(off + 8);
    const pix = u32(off + 12);
    const len = u32(off + 16);
    if (u16(off) !== IMAGE_W || u16(off + 2) !== IMAGE_H || rom[base + off + 4] !== 0 || len !== IMAGE_PIXELS) {
      errors.push(`${what}: bad image header`);
      return;
    }
    if (!span(pal, 512, tablesEnd, total) || !span(pix, len, tablesEnd, total)) {
      errors.push(`${what}: image data out of range`);
      return;
    }
    checkPalette(pal, what);
    if (photo) {
      const px = rom.subarray(base + pix, base + pix + len);
      for (let i = 0; i < px.length; i++) {
        if (px[i]! >= UI_FIRST) {
          errors.push(`${what}: photo pixels use UI slots`);
          break;
        }
      }
    }
  };

  const photoTable = u32(24);
  const pageTable = u32(28);
  if (!span(photoTable, photos * 28, 40, tablesEnd) || !span(pageTable, pages * 60, 40, tablesEnd)) {
    errors.push("tables out of range");
    return report;
  }
  for (let i = 0; i < photos; i++) {
    const off = photoTable + 28 * i;
    imageRef(off, `photo ${i}`, true);
    const printPal = u32(off + 20);
    if (rom[base + off + 24]! > 1) errors.push(`photo ${i}: bad orientation`);
    if (printPal) {
      if (span(printPal, 512, tablesEnd, total)) checkPalette(printPal, `photo ${i} print palette`);
      else errors.push(`photo ${i}: print palette out of range`);
    }
  }
  for (let p = 0; p < pages; p++) {
    const off = pageTable + 60 * p;
    imageRef(off, `page ${p}`, false);
    const count = rom[base + off + 22]!;
    if (u16(off + 20) !== 9 * p || count !== Math.min(9, photos - 9 * p)) errors.push(`page ${p}: wrong photos`);
    for (let c = 0; c < count; c++) {
      const o = base + off + 24 + 4 * c;
      const [x, y, w, h] = [rom[o]!, rom[o + 1]!, rom[o + 2]!, rom[o + 3]!];
      if (w !== 64 || h !== 60 || x < 4 || y < 4 || x + 68 > IMAGE_W || y + 64 > IMAGE_H) {
        errors.push(`page ${p} cell ${c}: bad geometry`);
      }
    }
  }
  return report;
}
