// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Append a photo pack to the template ROM and fix the cartridge header
// (docs/pack-format.md, "Placement in the ROM"). Byte-for-byte identical to
// cart/tools/lpcpack.py's patch_rom; web/test checks it against golden files.

import { loopyChecksum } from "./checksum.ts";
import { PACK_HEADER_SIZE, PACK_MAGIC, ROM_BASE, ROM_PAD_BLOCK } from "./constants.ts";
import { readDescriptor, TemplateError } from "./descriptor.ts";

export class PackTooLargeError extends Error {
  override name = "PackTooLargeError";
  constructor(
    readonly romBytes: number,
    readonly limitBytes: number,
  ) {
    super(`The ROM would be ${romBytes} bytes, but the cartridge holds ${limitBytes}.`);
  }
}

export class PackFormatError extends Error {
  override name = "PackFormatError";
}

export function buildRom(template: Uint8Array, pack: Uint8Array): Uint8Array {
  const descriptor = readDescriptor(template);

  if (pack.byteLength < PACK_HEADER_SIZE) {
    throw new PackFormatError("The photo pack is too short.");
  }
  const packView = new DataView(pack.buffer, pack.byteOffset, pack.byteLength);
  if (String.fromCharCode(...pack.subarray(0, 4)) !== PACK_MAGIC) {
    throw new PackFormatError("This is not a photo pack.");
  }
  const version = packView.getUint16(4);
  if (version !== descriptor.packVersion) {
    throw new TemplateError(
      `The template reads pack version ${descriptor.packVersion}, but this pack is version ${version}.`,
    );
  }
  if (packView.getUint32(8) !== pack.byteLength) {
    throw new PackFormatError("The photo pack's size field does not match its length.");
  }

  const packOffset = descriptor.packBase - ROM_BASE;
  if (template.byteLength > packOffset) {
    throw new TemplateError(
      `The template (${template.byteLength} bytes) overlaps the photo pack region.`,
    );
  }

  let length = packOffset + pack.byteLength;
  if (length % 2 !== 0) length += 1;
  const limitBytes = descriptor.romLimit - ROM_BASE;
  if (length > limitBytes) {
    throw new PackTooLargeError(length, limitBytes);
  }

  const padded = Math.ceil(length / ROM_PAD_BLOCK) * ROM_PAD_BLOCK;
  const rom = new Uint8Array(padded).fill(0xff);
  rom.set(template, 0);
  rom.set(pack, packOffset);

  const view = new DataView(rom.buffer);
  view.setUint32(4, ROM_BASE + length - 2);
  view.setUint32(8, loopyChecksum(rom));
  return rom;
}
