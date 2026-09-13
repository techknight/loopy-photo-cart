#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Build a test ROM from photo files: template + pack, ready for LoopyMSE.
#
# Usage:
#   python tools/mkfixture.py --out build/fixture.bin ../demo/photos/*.jpg
#   python tools/mkfixture.py --count 3 --no-dither --out build/three.bin ../demo/photos/*.jpg

import argparse
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lpcimage
import lpcpack

CART = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("photos", nargs="+", help="image files (globs allowed)")
    ap.add_argument("--template", default=os.path.join(CART, "build", "loopy-photo-cart-template.bin"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--count", type=int, default=lpcpack.MAX_PHOTOS,
                    help="use at most this many photos")
    ap.add_argument("--no-dither", action="store_true")
    ap.add_argument("--title", default="Fixture")
    args = ap.parse_args()

    paths = []
    for pattern in args.photos:
        matches = sorted(glob.glob(pattern))
        paths.extend(matches if matches else [pattern])
    paths = paths[:args.count]

    photos = []
    for path in paths:
        photo = lpcimage.load_photo(path, dither=not args.no_dither)
        kind = "portrait" if photo.orientation else "landscape"
        print(f"  {os.path.basename(path)}: {kind}")
        photos.append(photo)

    pack = lpcpack.encode_pack(photos, meta={"title": args.title, "tool": "mkfixture"})
    with open(args.template, "rb") as f:
        template = f.read()
    rom = lpcpack.patch_rom(template, pack)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(rom)
    print(f"{len(photos)} photos, pack {len(pack)} bytes, ROM {len(rom)} bytes -> {args.out}")


if __name__ == "__main__":
    main()
