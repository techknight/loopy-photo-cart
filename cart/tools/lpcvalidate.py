#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Validate a Loopy Photo Cart ROM the way the cartridge does (cart/src/pack.c),
# plus the encoder-side rules in docs/pack-format.md: header checksum, palette
# slot 0 black, UI slots 248-255, and photo pixels never in the UI slots.
#
# Usage: python cart/tools/lpcvalidate.py rom.bin

import struct
import sys
import zlib

import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lpcpack

W = lpcpack.IMAGE_W


def validate(rom):
    errors = []
    d = lpcpack.read_descriptor(rom)
    romlast = struct.unpack_from(">I", rom, 4)[0]
    rom_end = romlast + 2 - lpcpack.ROM_BASE
    if rom_end > len(rom):
        return [f"header romlast points past the file"], None
    if struct.unpack_from(">I", rom, 8)[0] != lpcpack.loopy_checksum(rom):
        errors.append("header checksum is wrong")
    if lpcpack.ROM_BASE + rom_end > d.rom_limit:
        errors.append("ROM exceeds the cartridge limit")

    base = d.pack_base - lpcpack.ROM_BASE
    if rom_end < base + 16:
        return errors + ["no photo pack"], None
    (magic, version, flags, total, crc, nphotos, npages, tables_end,
     photo_table, page_table, meta, cols, rows, _r) = struct.unpack_from(
        ">4sHHIIHHIIIIBBH", rom, base)
    if magic != lpcpack.PACK_MAGIC or version != lpcpack.PACK_VERSION:
        return errors + ["bad pack magic or version"], None
    if total < 40 or base + total > rom_end:
        return errors + ["pack size out of range"], None
    if tables_end & 3 or not 40 <= tables_end <= total:
        return errors + ["bad tables_end"], None
    if zlib.crc32(rom[base + 16:base + tables_end]) & 0xFFFFFFFF != crc:
        errors.append("header CRC mismatch")
    if nphotos > lpcpack.MAX_PHOTOS:
        errors.append("too many photos")
    if npages not in (0, -(-nphotos // 9)):
        errors.append("bad page count")
    if (cols, rows) != (3, 3):
        errors.append("grid must be 3x3")

    def span(off, length, lo, hi):
        return off % 4 == 0 and lo <= off <= hi and length <= hi - off

    def image_ref(off, what, photo):
        w, h, codec, _a, _b, pal, pix, length = struct.unpack_from(">HHBBHIII", rom, base + off)
        H = lpcpack.PHOTO_H if photo else lpcpack.PAGE_H
        if (w, h, codec, length) != (W, H, 0, W * H):
            errors.append(f"{what}: bad image header")
            return
        if not span(pal, 512, tables_end, total) or not span(pix, length, tables_end, total):
            errors.append(f"{what}: image data out of range")
            return
        check_palette(pal, what)
        if photo and max(rom[base + pix:base + pix + length]) >= lpcpack.UI_FIRST:
            errors.append(f"{what}: photo pixels use UI slots")

    def check_palette(off, what):
        entries = struct.unpack_from(">256H", rom, base + off)
        if entries[0] != 0:
            errors.append(f"{what}: palette slot 0 is not black")
        if list(entries[lpcpack.UI_FIRST:]) != lpcpack.UI_PALETTE:
            errors.append(f"{what}: UI slots 248-255 are wrong")

    if not span(photo_table, nphotos * 28, 40, tables_end) or \
            not span(page_table, npages * 60, 40, tables_end):
        return errors + ["tables out of range"], None
    for i in range(nphotos):
        off = photo_table + 28 * i
        image_ref(off, f"photo {i}", True)
        print_pal, orient = struct.unpack_from(">IB", rom, base + off + 20)
        if orient > 1:
            errors.append(f"photo {i}: bad orientation")
        if print_pal:
            if span(print_pal, 512, tables_end, total):
                check_palette(print_pal, f"photo {i} print palette")
            else:
                errors.append(f"photo {i}: print palette out of range")
    for p in range(npages):
        off = page_table + 60 * p
        image_ref(off, f"page {p}", False)
        first, count = struct.unpack_from(">HB", rom, base + off + 20)
        if first != 9 * p or count != min(9, nphotos - 9 * p):
            errors.append(f"page {p}: wrong photos")
        for c in range(count):
            x, y, w, h = struct.unpack_from("4B", rom, base + off + 24 + 4 * c)
            if (w, h) != (64, 60) or x < 4 or y < 4 or x + 68 > W or y + 64 > lpcpack.PAGE_H:
                errors.append(f"page {p} cell {c}: bad geometry")
    return errors, (nphotos, npages, total, len(rom))


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: lpcvalidate.py rom.bin")
    with open(sys.argv[1], "rb") as f:
        rom = f.read()
    errors, info = validate(rom)
    if info:
        print(f"{info[0]} photos, {info[1]} pages, pack {info[2]} bytes, ROM {info[3]} bytes")
    for e in errors:
        print("ERROR:", e)
    if errors:
        sys.exit(1)
    print("ROM is valid")


if __name__ == "__main__":
    main()
