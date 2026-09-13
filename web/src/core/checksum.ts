// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later

import { ROM_BASE } from "./constants.ts";

/**
 * The Casio Loopy cartridge header checksum, as cart/tools/fixrom.py computes
 * it: the 32-bit wrapping sum of big-endian u16 words from `romfirst` (header
 * offset 0) through `romlast + 1` (romlast is at header offset 4).
 */
export function loopyChecksum(rom: Uint8Array): number {
  const view = new DataView(rom.buffer, rom.byteOffset, rom.byteLength);
  const start = view.getUint32(0) - ROM_BASE;
  const end = view.getUint32(4) + 2 - ROM_BASE;

  if (start < 0 || end > rom.byteLength || start > end || (end - start) % 2 !== 0) {
    throw new RangeError("ROM header checksum range is outside the image");
  }

  let sum = 0;
  for (let i = start; i < end; i += 2) {
    sum = (sum + view.getUint16(i)) >>> 0;
  }
  return sum;
}
