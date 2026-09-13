#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Build a calibration ROM for the sticker printer (docs/hardware-test.md).
#
# Two test charts, printed and measured, settle what the web app's crop box
# needs (docs/PLAN.md section 8):
#
#   1  LANDSCAPE  the stored 256x224 photo as the printer receives it
#   2  PORTRAIT   an upright chart stored rotated 90 degrees clockwise, the way
#                 portrait photos are, to show which way up it prints
#
# Each chart has a 1 px grid every 16 px (bold every 64), a red border on the
# outermost pixels, a circle drawn to print round (so it also looks round in
# the viewer), and a horizontal and a vertical bar each exactly 200 px long.
# Measured on the sticker:
#
#   whole image width / height  -> the printed shape (STICKER_ASPECT)
#   horizontal bar / vertical   -> the printer's dot shape
#
# Usage: python tools/mkcalibration.py [--out build/calibration.bin]

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw, ImageFont

import lpcimage
import lpcpack

CART = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
BLUE = (0, 64, 255)
GREEN = (0, 160, 0)
GREY = (170, 170, 170)


def chart(width, height, label, up_arrow):
    img = Image.new("RGB", (width, height), WHITE)
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(lpcimage.FONT_PATH, lpcimage.FONT_SIZE)
    small = ImageFont.truetype(lpcimage.SMALL_FONT_PATH, lpcimage.SMALL_FONT_SIZE)

    for x in range(0, width, 16):
        d.line([(x, 0), (x, height - 1)], fill=BLACK if x % 64 == 0 else GREY)
    for y in range(0, height, 16):
        d.line([(0, y), (width - 1, y)], fill=BLACK if y % 64 == 0 else GREY)

    d.rectangle([0, 0, width - 1, height - 1], outline=RED, width=2)

    # A circle that prints round: dots are wider than tall, so it takes more
    # dots down than across. A portrait chart's x runs along the printer's
    # lines, so there it takes more dots across.
    cx, cy = width // 2, height // 2
    stretch = lpcpack.DOT_W_MM / lpcpack.DOT_H_MM
    rx, ry = (90, round(90 * stretch)) if width > height else (round(90 * stretch), 90)
    d.ellipse([cx - rx, cy - ry, cx + rx - 1, cy + ry - 1], outline=BLUE, width=2)

    # The measuring bars: exactly 200 px, 3 px thick, with end ticks.
    d.rectangle([cx - 100, cy - 1, cx + 99, cy + 1], fill=GREEN)
    d.rectangle([cx - 1, cy - 100, cx + 1, cy + 99], fill=GREEN)
    for t in (cx - 100, cx + 99):
        d.line([(t, cy - 6), (t, cy + 6)], fill=GREEN, width=1)
    for t in (cy - 100, cy + 99):
        d.line([(cx - 6, t), (cx + 6, t)], fill=GREEN, width=1)

    d.text((6, 6 - lpcimage.FONT_TOP), label, font=font, fill=BLACK)
    d.text((6, height - 12), "bars: 200 px", font=small, fill=BLACK)
    if up_arrow:
        ax = width - 22
        d.polygon([(ax, 8), (ax - 8, 22), (ax + 8, 22)], fill=RED)
        d.rectangle([ax - 2, 22, ax + 2, 40], fill=RED)
        d.text((ax - 8, 44), "UP", font=small, fill=RED)
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--template", default=os.path.join(CART, "build", "loopy-photo-cart-template.bin"))
    ap.add_argument("--out", default=os.path.join(CART, "build", "calibration.bin"))
    args = ap.parse_args()

    # Drawn directly in printer dots, so the bars measure the dots themselves.
    landscape = chart(lpcpack.IMAGE_W, lpcpack.PHOTO_H, "1 LANDSCAPE", up_arrow=True)
    portrait_up = chart(lpcpack.PHOTO_H, lpcpack.IMAGE_W, "2 PORTRAIT", up_arrow=True)
    portrait_stored = portrait_up.transpose(Image.Transpose.ROTATE_270)

    photos = [
        lpcpack.Photo(image=lpcimage.quantize(landscape, dither=False)),
        lpcpack.Photo(image=lpcimage.quantize(portrait_stored, dither=False),
                      orientation=lpcpack.ORIENT_PORTRAIT),
    ]
    thumbs = [lpcimage.thumbnail(landscape), lpcimage.thumbnail(portrait_up)]
    pages = lpcimage.render_pages(thumbs)

    pack = lpcpack.encode_pack(photos, pages, meta={"title": "Calibration", "tool": "mkcalibration"})
    with open(args.template, "rb") as f:
        rom = lpcpack.patch_rom(f.read(), pack)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(rom)
    print(f"calibration ROM: {len(rom)} bytes -> {args.out}")


if __name__ == "__main__":
    main()
