#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Photo conversion for the fixture tools: crop to the sticker's shape,
# resample to 256x224 printer dots, quantize to RGB555 around the reserved
# slots, and render
# the 3x3 grid pages. This is a stand-in for the web app's pipeline
# (docs/PLAN.md, Phase 4), good enough to put real photos on the cart for
# testing. Requires Pillow.

import os

from PIL import Image as PILImage, ImageDraw, ImageFont, ImageOps

import lpcpack

CART = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_PATH = os.path.join(CART, "assets", "font", "home-video", "HomeVideo-Regular.ttf")
FONT_SIZE = 20       # Home Video's native pixel size (tools/mkfont.py)
FONT_TOP = 2         # first inked row below the draw origin
FONT_ADVANCE = 12

# Public Pixel Font (GGBotNet, CC0): 8 px cell, mixed case, for small print.
SMALL_FONT_PATH = os.path.join(CART, "assets", "font", "public-pixel", "PublicPixel.ttf")
SMALL_FONT_SIZE = 8
SMALL_FONT_TOP = 1
SMALL_FONT_ADVANCE = 8

# The printed shape of a photo, measured on hardware (lpcpack.STICKER_ASPECT,
# about 1.283 : 1).
STICKER_ASPECT = lpcpack.STICKER_ASPECT

# Slot 0 is black (it doubles as the backdrop) and 248..255 are the UI's, so a
# photo gets slots 1..247.
PHOTO_COLOURS = lpcpack.UI_FIRST - 1

# Grid page layout (docs/pack-format.md, PageEntry). Thumbnails are exactly a
# quarter of a photo; 16 px gaps across, and a header row above.
THUMB_W, THUMB_H = 64, 60
CELL_XS = (16, 96, 176)
CELL_YS = (34, 100, 166)
HEADER_Y = 14
HEADER_X = 16
HEADER_RIGHT = 240
HEADER_TITLE = "PHOTOS"
# Control hints in Public Pixel, two lines between the title (which ends at
# x = 88) and the page counter (which starts at x = 204 for "6/6").
CONTROLS_X = 100
CONTROLS_YS = (13, 22)
# Public Pixel has real lowercase, so the hints are mixed case. L/R paging
# is left to the help popover D opens (docs/PLAN.md, Phase 5).
CONTROLS = ("A: View", "D: Show Help")

UI_BLACK, UI_BG, UI_PANEL, UI_DIM, UI_GREY, UI_WHITE, UI_ACCENT, UI_HIGHLIGHT = range(248, 256)


def _expand5(v5):
    return (v5 << 3) | (v5 >> 2)


def _posterize_lut():
    # Snap every channel to the nearest 5-bit level, expressed back in 8 bits,
    # so the quantizer works on colours the Loopy can actually show.
    lut = [_expand5(min(31, (v * 31 + 127) // 255)) for v in range(256)]
    return lut * 3


def crop_box(img, portrait):
    """Centre-crop to the sticker's printed shape, at the photo's own pixels."""
    aspect = 1 / STICKER_ASPECT if portrait else STICKER_ASPECT
    w, h = img.size
    if w / h > aspect:
        cw = round(h * aspect)
        box = ((w - cw) // 2, 0, (w - cw) // 2 + cw, h)
    else:
        ch = round(w / aspect)
        box = (0, (h - ch) // 2, w, (h - ch) // 2 + ch)
    return img.crop(box)


def to_dots(crop, portrait):
    """Resample a crop to printer dots, upright: 256x224, or 224x256 portrait.

    The dots are not square, so the two axes scale by different amounts; that
    is what makes the sticker show the crop's true shape."""
    size = (lpcpack.PHOTO_H, lpcpack.IMAGE_W) if portrait else (lpcpack.IMAGE_W, lpcpack.PHOTO_H)
    return crop.resize(size, PILImage.Resampling.LANCZOS, reducing_gap=3.0)


def quantize(img, dither=True):
    """RGB image (a 256-wide photo or page) -> lpcpack.Image, slots 1..247 only."""
    post = img.convert("RGB").point(_posterize_lut())
    first = post.quantize(colors=PHOTO_COLOURS, method=PILImage.Quantize.MEDIANCUT,
                          dither=PILImage.Dither.NONE)
    photo_rgb = first.getpalette()[:PHOTO_COLOURS * 3]
    photo_rgb += [0] * (PHOTO_COLOURS * 3 - len(photo_rgb))
    rgb = [0, 0, 0] + photo_rgb
    for r, g, b in lpcpack.UI_COLOURS:
        rgb += [_expand5(r), _expand5(g), _expand5(b)]

    # Map (and dither) against the photo's own slots only. Were the UI slots
    # in this palette, error diffusion would reach for them -- the highlight
    # yellow showed as speckles on orange fur -- so they are stood in for by
    # copies of slot 1 and folded back onto it afterwards.
    map_rgb = rgb[:lpcpack.UI_FIRST * 3] + rgb[3:6] * (256 - lpcpack.UI_FIRST)
    pal_img = PILImage.new("P", (1, 1))
    pal_img.putpalette(map_rgb)
    mapped = post.quantize(palette=pal_img,
                           dither=PILImage.Dither.FLOYDSTEINBERG if dither else PILImage.Dither.NONE)
    fold = bytes(range(lpcpack.UI_FIRST)) + bytes([1]) * (256 - lpcpack.UI_FIRST)

    palette = []
    for i in range(256):
        r, g, b = rgb[i * 3:i * 3 + 3]
        palette.append(lpcpack.rgb555(r >> 3, g >> 3, b >> 3))
    return lpcpack.Image(palette=palette, pixels=mapped.tobytes().translate(fold))


def load_photo(path, dither=True):
    """A photo file -> (lpcpack.Photo, the upright crop at its own pixels).

    The crop is returned unresampled, so thumbnails (square screen pixels)
    are cut from the true picture rather than from the printer-dot copy."""
    with PILImage.open(path) as im:
        img = ImageOps.exif_transpose(im).convert("RGB")
    portrait = img.height > img.width
    upright = crop_box(img, portrait)
    dots = to_dots(upright, portrait)
    # Portrait photos are stored 90 degrees clockwise: 224x256 -> 256x224.
    stored = dots.transpose(PILImage.Transpose.ROTATE_270) if portrait else dots
    photo = lpcpack.Photo(
        image=quantize(stored, dither),
        orientation=lpcpack.ORIENT_PORTRAIT if portrait else lpcpack.ORIENT_LANDSCAPE,
    )
    return photo, upright


def thumbnail(upright):
    """Fill a 64x60 cell from an upright sticker crop."""
    return ImageOps.fit(upright, (THUMB_W, THUMB_H), PILImage.Resampling.LANCZOS)


def _draw_font(pixels, font_path, size, top, x, y, text, colour):
    font = ImageFont.truetype(font_path, size)
    mask = PILImage.new("1", (lpcpack.IMAGE_W, lpcpack.IMAGE_H), 0)
    d = ImageDraw.Draw(mask)
    d.fontmode = "1"
    d.text((x, y - top), text, font=font, fill=1)
    for i, on in enumerate(mask.getdata()):
        if on:
            pixels[i] = colour


def _draw_text(pixels, x, y, text, colour):
    """Home Video Font; (x, y) is the top-left of the ink."""
    _draw_font(pixels, FONT_PATH, FONT_SIZE, FONT_TOP, x, y, text, colour)


def _draw_small_text(pixels, x, y, text, colour):
    """Public Pixel Font; (x, y) is the top-left of the ink.

    Drawn a character at a time so spacing can differ from the monospaced
    cell: a space after a colon ("A: View") is half a cell, since a full
    8 px gap reads as too wide there."""
    prev = ""
    for ch in text:
        if ch == " " and prev == ":":
            x += SMALL_FONT_ADVANCE // 2
        else:
            if ch != " ":
                _draw_font(pixels, SMALL_FONT_PATH, SMALL_FONT_SIZE, SMALL_FONT_TOP,
                           x, y, ch, colour)
            x += SMALL_FONT_ADVANCE
        prev = ch


def render_pages(thumbs, dither=False):
    """Upright thumbnails (in photo order) -> [lpcpack.Page].

    Not dithered by default: nine photos share one palette, and at 64x60
    error diffusion leaves white specks in skies and night shots; flat
    mapping looks cleaner at thumbnail size."""
    per_page = lpcpack.CELLS_PER_PAGE
    page_count = -(-len(thumbs) // per_page)
    pages = []

    for p in range(page_count):
        chunk = thumbs[p * per_page:(p + 1) * per_page]
        cells = [(CELL_XS[i % 3], CELL_YS[i // 3], THUMB_W, THUMB_H) for i in range(len(chunk))]

        canvas = PILImage.new("RGB", (lpcpack.IMAGE_W, lpcpack.IMAGE_H), (0, 0, 0))
        for thumb, (x, y, _w, _h) in zip(chunk, cells):
            canvas.paste(thumb, (x, y))
        image = quantize(canvas, dither)

        # Everything outside the thumbnails is UI: background, then header.
        pixels = bytearray(image.pixels)
        inside = bytearray(lpcpack.IMAGE_W * lpcpack.IMAGE_H)
        for x, y, w, h in cells:
            for row in range(y, y + h):
                inside[row * lpcpack.IMAGE_W + x:row * lpcpack.IMAGE_W + x + w] = b"\1" * w
        for i, flag in enumerate(inside):
            if not flag:
                pixels[i] = UI_BG

        _draw_text(pixels, HEADER_X, HEADER_Y, HEADER_TITLE, UI_ACCENT)
        for line, y in zip(CONTROLS, CONTROLS_YS):
            _draw_small_text(pixels, CONTROLS_X, y, line, UI_WHITE)
        counter = f"{p + 1}/{page_count}"
        _draw_text(pixels, HEADER_RIGHT - FONT_ADVANCE * len(counter), HEADER_Y, counter, UI_GREY)

        pages.append(lpcpack.Page(
            image=lpcpack.Image(palette=image.palette, pixels=bytes(pixels)),
            first_photo=p * per_page,
            cells=cells,
        ))
    return pages
