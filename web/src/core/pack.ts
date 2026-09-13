// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Photo pack encoder (docs/pack-format.md). Byte-for-byte identical to
// cart/tools/lpcpack.py's encode_pack; web/test/pack.test.ts holds it to that.

import { crc32 } from "./crc32.ts";
import { PACK_HEADER_SIZE, PACK_MAGIC, PACK_VERSION } from "./constants.ts";
import {
  CELLS_PER_PAGE,
  CURSOR_RING,
  IMAGE_H,
  IMAGE_PIXELS,
  IMAGE_W,
  MAX_PHOTOS,
  ORIENT_LANDSCAPE,
  ORIENT_PORTRAIT,
  THUMB_H,
  THUMB_W,
  UI_FIRST,
  UI_PALETTE,
} from "./palette.ts";

export interface PackImage {
  palette: ArrayLike<number>;
  pixels: Uint8Array;
}

export interface PackPhoto {
  image: PackImage;
  orientation: number;
  printPalette?: ArrayLike<number>;
}

export type Cell = readonly [x: number, y: number, w: number, h: number];

export interface PackPage {
  image: PackImage;
  firstPhoto: number;
  cells: readonly Cell[];
}

export class PackError extends Error {
  override name = "PackError";
}

const PHOTO_SIZE = 28;
const PAGE_SIZE = 60;
const GRID = 3;
const FLAG_PRINT_PALETTES = 1;

function align4(n: number): number {
  return (n + 3) & ~3;
}

function checkPalette(p: ArrayLike<number>, what: string): void {
  if (p.length !== 256) throw new PackError(`${what}: palette has ${p.length} entries, not 256`);
  for (let i = 0; i < 256; i++) {
    if (!(p[i]! >= 0 && p[i]! <= 0x7fff)) throw new PackError(`${what}: palette entry out of RGB555 range`);
  }
  if (p[0] !== 0) throw new PackError(`${what}: slot 0 must be black (it is also the backdrop)`);
  for (let i = 0; i < 8; i++) {
    if (p[UI_FIRST + i] !== UI_PALETTE[i]) {
      throw new PackError(`${what}: slots 248-255 must be the reserved UI colours`);
    }
  }
}

function checkImage(img: PackImage, what: string): void {
  checkPalette(img.palette, what);
  if (img.pixels.length !== IMAGE_PIXELS) {
    throw new PackError(`${what}: ${img.pixels.length} pixels, not ${IMAGE_PIXELS}`);
  }
}

class Writer {
  bytes = new Uint8Array(1 << 16);
  length = 0;

  ensure(extra: number): void {
    if (this.length + extra <= this.bytes.length) return;
    let size = this.bytes.length;
    while (size < this.length + extra) size *= 2;
    const next = new Uint8Array(size);
    next.set(this.bytes.subarray(0, this.length));
    this.bytes = next;
  }

  append(data: Uint8Array): void {
    this.ensure(data.length);
    this.bytes.set(data, this.length);
    this.length += data.length;
  }

  padTo4(): void {
    const target = align4(this.length);
    this.ensure(target - this.length);
    this.bytes.fill(0, this.length, target);
    this.length = target;
  }
}

function paletteBytes(p: ArrayLike<number>): Uint8Array {
  const out = new Uint8Array(512);
  const v = new DataView(out.buffer);
  for (let i = 0; i < 256; i++) v.setUint16(i * 2, p[i]!);
  return out;
}

export function encodePack(
  photos: readonly PackPhoto[],
  pages: readonly PackPage[] = [],
  meta: Readonly<Record<string, string>> = {},
): Uint8Array {
  if (photos.length > MAX_PHOTOS) throw new PackError(`${photos.length} photos; at most ${MAX_PHOTOS}`);
  if (pages.length && pages.length !== Math.ceil(photos.length / CELLS_PER_PAGE)) {
    throw new PackError("page count must be 0 or ceil(photos / 9)");
  }
  photos.forEach((p, i) => {
    checkImage(p.image, `photo ${i}`);
    if (p.orientation !== ORIENT_LANDSCAPE && p.orientation !== ORIENT_PORTRAIT) {
      throw new PackError(`photo ${i}: bad orientation ${p.orientation}`);
    }
    if (p.printPalette) checkPalette(p.printPalette, `photo ${i} print palette`);
  });
  pages.forEach((pg, i) => {
    checkImage(pg.image, `page ${i}`);
    const expected = Math.min(CELLS_PER_PAGE, photos.length - i * CELLS_PER_PAGE);
    if (pg.firstPhoto !== i * CELLS_PER_PAGE || pg.cells.length !== expected) {
      throw new PackError(`page ${i}: must hold photos ${i * CELLS_PER_PAGE}.. in ${expected} cells`);
    }
    for (const [x, y, w, h] of pg.cells) {
      if (w !== THUMB_W || h !== THUMB_H || x < CURSOR_RING || y < CURSOR_RING ||
          x + w + CURSOR_RING > IMAGE_W || y + h + CURSOR_RING > IMAGE_H) {
        throw new PackError(`page ${i}: cell is not a 64x60 thumbnail with room for the cursor ring`);
      }
    }
  });

  const utf8 = new TextEncoder();
  const metaParts: Uint8Array[] = [];
  for (const [key, value] of Object.entries(meta)) {
    if (!key || key.includes("=") || (key + value).includes("\0")) throw new PackError(`bad meta key ${key}`);
    metaParts.push(utf8.encode(`${key}=${value}`), new Uint8Array(1));
  }
  metaParts.push(new Uint8Array(1));
  const metaLength = metaParts.reduce((s, p) => s + p.length, 0);

  const photoTable = PACK_HEADER_SIZE;
  const pageTable = photoTable + PHOTO_SIZE * photos.length;
  const metaOffset = pageTable + PAGE_SIZE * pages.length;
  const tablesEnd = align4(metaOffset + metaLength);

  const blobs = new Writer();
  const put = (data: Uint8Array): number => {
    const off = tablesEnd + blobs.length;
    blobs.append(data);
    blobs.padTo4();
    return off;
  };
  const putImage = (img: PackImage): Uint8Array => {
    const paletteOff = put(paletteBytes(img.palette));
    const pixelsOff = put(img.pixels);
    const ref = new Uint8Array(20);
    const v = new DataView(ref.buffer);
    v.setUint16(0, IMAGE_W);
    v.setUint16(2, IMAGE_H);
    v.setUint32(8, paletteOff);
    v.setUint32(12, pixelsOff);
    v.setUint32(16, img.pixels.length);
    return ref;
  };

  const tables = new Uint8Array(tablesEnd - PACK_HEADER_SIZE);
  const tv = new DataView(tables.buffer);
  let t = 0;
  for (const p of photos) {
    tables.set(putImage(p.image), t);
    const printOff = p.printPalette ? put(paletteBytes(p.printPalette)) : 0;
    tv.setUint32(t + 20, printOff);
    tv.setUint8(t + 24, p.orientation);
    t += PHOTO_SIZE;
  }
  for (const pg of pages) {
    tables.set(putImage(pg.image), t);
    tv.setUint16(t + 20, pg.firstPhoto);
    tv.setUint8(t + 22, pg.cells.length);
    pg.cells.forEach(([x, y, w, h], c) => tables.set([x, y, w, h], t + 24 + c * 4));
    t += PAGE_SIZE;
  }
  for (const part of metaParts) {
    tables.set(part, t);
    t += part.length;
  }

  const total = tablesEnd + blobs.length;
  const pack = new Uint8Array(total);
  const hv = new DataView(pack.buffer);
  for (let i = 0; i < 4; i++) pack[i] = PACK_MAGIC.charCodeAt(i);
  hv.setUint16(4, PACK_VERSION);
  hv.setUint16(6, photos.some((p) => p.printPalette) ? FLAG_PRINT_PALETTES : 0);
  hv.setUint32(8, total);
  hv.setUint16(16, photos.length);
  hv.setUint16(18, pages.length);
  hv.setUint32(20, tablesEnd);
  hv.setUint32(24, photoTable);
  hv.setUint32(28, pageTable);
  hv.setUint32(32, metaOffset);
  hv.setUint8(36, GRID);
  hv.setUint8(37, GRID);
  pack.set(tables, PACK_HEADER_SIZE);
  pack.set(blobs.bytes.subarray(0, blobs.length), tablesEnd);
  hv.setUint32(12, crc32(pack.subarray(16, tablesEnd)));
  return pack;
}
