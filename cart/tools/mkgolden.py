#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Regenerate the golden files the web app's tests compare against
# (web/test/fixtures). The pack is synthetic, so this needs only the standard
# library and a built template.
#
#   template.bin  a frozen copy of the cartridge template
#   pack.bin      a small pack: a landscape photo, and a portrait photo with a
#                 print palette, plus meta strings
#   rom.bin       template + pack, patched by lpcpack.patch_rom
#   golden.json   the header values, for readable assertions
#
# rom.bin's checksum is cross-checked against tools/fixrom.py before anything
# is written.
#
# Usage: python tools/mkgolden.py [--template build/loopy-photo-cart-template.bin]

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lpcpack

CART = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(CART)
OUT = os.path.join(REPO, "web", "test", "fixtures")


def synthetic_image(seed):
    palette = [0] + [lpcpack.rgb555((i >> 3) & 31, ((255 - i) >> 3) & 31, (i * seed) & 31)
                     for i in range(1, lpcpack.UI_FIRST)] + lpcpack.UI_PALETTE
    pixels = bytes(((x ^ (y * seed)) + seed) % lpcpack.UI_FIRST
                   for y in range(lpcpack.IMAGE_H) for x in range(lpcpack.IMAGE_W))
    return lpcpack.Image(palette=palette, pixels=pixels)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--template", default=os.path.join(CART, "build", "loopy-photo-cart-template.bin"))
    args = ap.parse_args()

    with open(args.template, "rb") as f:
        template = f.read()

    print_palette = [0] + [lpcpack.rgb555(31 - (i >> 3), i >> 3, 15)
                           for i in range(1, lpcpack.UI_FIRST)]
    photos = [
        lpcpack.Photo(image=synthetic_image(3)),
        lpcpack.Photo(image=synthetic_image(5), orientation=lpcpack.ORIENT_PORTRAIT,
                      print_palette=print_palette + lpcpack.UI_PALETTE),
    ]
    pack = lpcpack.encode_pack(photos, meta={"title": "Golden", "tool": "mkgolden"})
    rom = lpcpack.patch_rom(template, pack)

    # Cross-check the checksum with the cartridge build's own fixer.
    with tempfile.TemporaryDirectory() as tmp:
        probe = os.path.join(tmp, "probe.bin")
        with open(probe, "wb") as f:
            f.write(rom)
        subprocess.run([sys.executable, os.path.join(CART, "tools", "fixrom.py"), probe],
                       check=True, stdout=subprocess.DEVNULL)
        with open(probe, "rb") as f:
            if f.read() != rom:
                sys.exit("lpcpack checksum disagrees with tools/fixrom.py")

    tables_end = struct.unpack_from(">I", pack, 20)[0]
    golden = {
        "templateBuildId": lpcpack.read_descriptor(template).build_id,
        "packSize": len(pack),
        "tablesEnd": tables_end,
        "headerCrc32": zlib.crc32(pack[16:tables_end]) & 0xFFFFFFFF,
        "romSize": len(rom),
        "romLast": struct.unpack_from(">I", rom, 4)[0],
        "checksum": struct.unpack_from(">I", rom, 8)[0],
    }

    os.makedirs(OUT, exist_ok=True)
    shutil.copyfile(args.template, os.path.join(OUT, "template.bin"))
    for name, data in (("pack.bin", pack), ("rom.bin", rom)):
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(data)
    with open(os.path.join(OUT, "golden.json"), "w", encoding="ascii", newline="\n") as f:
        json.dump(golden, f, indent=2)
        f.write("\n")
    print(json.dumps(golden, indent=2))


if __name__ == "__main__":
    main()
