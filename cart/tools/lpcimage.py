#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Photo conversion for the fixture tools: crop to the sticker's shape,
# resize to 256x240, quantize to RGB555 with the reserved UI slots. This is a
# stand-in for the web app's pipeline (docs/PLAN.md, Phase 4), good enough to
# put real photos on the cart for testing. Requires Pillow.

from PIL import Image as PILImage, ImageOps

import lpcpack

# Placeholder until the Phase 3 hardware test measures the printed sticker
# (docs/PLAN.md section 8, STICKER_ASPECT).
STICKER_ASPECT = 4 / 3

# Slot 0 is black (it doubles as the backdrop) and 248..255 are the UI's, so a
# photo gets slots 1..247.
PHOTO_COLOURS = lpcpack.UI_FIRST - 1


def _expand5(v5):
    return (v5 << 3) | (v5 >> 2)


def _posterize_lut():
    # Snap every channel to the nearest 5-bit level, expressed back in 8 bits,
    # so the quantizer works on colours the Loopy can actually show.
    lut = [_expand5(min(31, (v * 31 + 127) // 255)) for v in range(256)]
    return lut * 3


def crop_fill(img, portrait):
    """Centre-crop to the sticker's shape and resize to the stored size."""
    aspect = 1 / STICKER_ASPECT if portrait else STICKER_ASPECT
    w, h = img.size
    if w / h > aspect:
        cw = round(h * aspect)
        box = ((w - cw) // 2, 0, (w - cw) // 2 + cw, h)
    else:
        ch = round(w / aspect)
        box = (0, (h - ch) // 2, w, (h - ch) // 2 + ch)
    size = (lpcpack.IMAGE_H, lpcpack.IMAGE_W) if portrait else (lpcpack.IMAGE_W, lpcpack.IMAGE_H)
    img = img.crop(box).resize(size, PILImage.Resampling.LANCZOS, reducing_gap=3.0)
    if portrait:
        # 90 degrees clockwise: the upright 240x256 picture becomes 256x240.
        img = img.transpose(PILImage.Transpose.ROTATE_270)
    return img


def quantize(img, dither=True):
    """RGB image (256x240) -> lpcpack.Image."""
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
    """A photo file -> lpcpack.Photo, filling the sticker."""
    with PILImage.open(path) as im:
        img = ImageOps.exif_transpose(im).convert("RGB")
    portrait = img.height > img.width
    img = crop_fill(img, portrait)
    return lpcpack.Photo(
        image=quantize(img, dither),
        orientation=lpcpack.ORIENT_PORTRAIT if portrait else lpcpack.ORIENT_LANDSCAPE,
    )
