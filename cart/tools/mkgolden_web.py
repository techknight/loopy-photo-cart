#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Golden files for the web app's pack encoder and page text renderer:
#
#   web/test/fixtures/pack-pages.bin  lpcpack.encode_pack over three synthetic
#                                     photos (one portrait with a print
#                                     palette) and one grid page, with meta
#   web/test/fixtures/header.bin      a 256x240 page of UI_BG with the grid
#                                     header drawn by lpcimage ("PHOTOS", the
#                                     hints, "1/6")
#
# The synthetic images come from formulas web/test/pack.test.ts repeats, so the
# TypeScript encoder must produce identical bytes from identical inputs.
#
# Usage: python cart/tools/mkgolden_web.py

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lpcimage
import lpcpack

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "web", "test", "fixtures")

W, H = lpcpack.IMAGE_W, lpcpack.IMAGE_H


def palette(seed):
    return ([0] + [lpcpack.rgb555((i * seed) & 31, (i >> 3) & 31, ((255 - i) >> 3) & 31)
                   for i in range(1, lpcpack.UI_FIRST)] + lpcpack.UI_PALETTE)


def pixels(seed):
    return bytes(((x * seed + y) % 247) + 1 for y in range(H) for x in range(W))


def image(seed):
    return lpcpack.Image(palette=palette(seed), pixels=pixels(seed))


def main():
    print_palette = ([0] + [lpcpack.rgb555(31 - (i >> 3), i >> 3, 15)
                            for i in range(1, lpcpack.UI_FIRST)] + lpcpack.UI_PALETTE)
    photos = [
        lpcpack.Photo(image=image(3)),
        lpcpack.Photo(image=image(5), orientation=lpcpack.ORIENT_PORTRAIT,
                      print_palette=print_palette),
        lpcpack.Photo(image=image(11)),
    ]
    cells = [(lpcimage.CELL_XS[i % 3], lpcimage.CELL_YS[i // 3], 64, 60) for i in range(3)]
    pages = [lpcpack.Page(image=image(7), first_photo=0, cells=cells)]
    pack = lpcpack.encode_pack(photos, pages, meta={
        "title": "Golden", "tool": "mkgolden_web", "created": "2026-09-13"})

    header = bytearray([lpcimage.UI_BG]) * (W * H)
    lpcimage._draw_text(header, lpcimage.HEADER_X, lpcimage.HEADER_Y, lpcimage.HEADER_TITLE,
                        lpcimage.UI_ACCENT)
    for line, y in zip(lpcimage.CONTROLS, lpcimage.CONTROLS_YS):
        lpcimage._draw_small_text(header, lpcimage.CONTROLS_X, y, line, lpcimage.UI_WHITE)
    lpcimage._draw_text(header, lpcimage.HEADER_RIGHT - lpcimage.FONT_ADVANCE * 3,
                        lpcimage.HEADER_Y, "1/6", lpcimage.UI_GREY)

    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "pack-pages.bin"), "wb") as f:
        f.write(pack)
    with open(os.path.join(OUT, "header.bin"), "wb") as f:
        f.write(header)
    print(f"pack-pages.bin {len(pack)} bytes, header.bin {len(header)} bytes")


if __name__ == "__main__":
    main()
