// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// ROM layout and pack format constants (docs/pack-format.md).

export const ROM_BASE = 0x0e000000;
export const ROM_PAD_BLOCK = 4096;

export const DESCRIPTOR_OFFSET = 0x20;
export const DESCRIPTOR_SIZE = 32;
export const DESCRIPTOR_MAGIC = "LPCT";
export const DESCRIPTOR_VERSION = 1;

export const PACK_MAGIC = "LPCP";
export const PACK_VERSION = 1;
export const PACK_HEADER_SIZE = 40;
