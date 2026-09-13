#!/usr/bin/env python3
"""Make publishable copies of the demo photos.

For every JPEG/PNG in SRC this writes DST/<same name>.jpg that is:
  - rotated upright (EXIF orientation applied, then discarded),
  - converted from its embedded colour profile (e.g. Display P3) to sRGB,
  - downscaled to at most --long-edge pixels on the long side,
  - stripped of ALL metadata (EXIF, GPS, XMP, ICC, comments).

Each output is re-read and checked for leftover metadata; any hit is fatal.

Usage: python demo/prepare_photos.py originals/ demo/photos [--long-edge 1600]
"""

import argparse
import io
import sys
from pathlib import Path

from PIL import Image, ImageCms, ImageOps

SRGB = ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB"))
SUSPECT_BYTES = (b"Exif", b"http://ns.adobe.com", b"GPS", b"ICC_PROFILE", b"Apple", b"iPhone")


def clean(src: Path, dst: Path, long_edge: int, quality: int) -> None:
    with Image.open(src) as im:
        icc = im.info.get("icc_profile")
        im = ImageOps.exif_transpose(im).convert("RGB")
        if icc:
            im = ImageCms.profileToProfile(
                im, ImageCms.ImageCmsProfile(io.BytesIO(icc)), SRGB,
                renderingIntent=ImageCms.Intent.PERCEPTUAL, outputMode="RGB")
        scale = long_edge / max(im.size)
        if scale < 1:
            size = (round(im.width * scale), round(im.height * scale))
            im = im.resize(size, Image.Resampling.LANCZOS, reducing_gap=3.0)
        # A fresh image carries no .info, so nothing from the source can leak.
        out = Image.new("RGB", im.size)
        out.paste(im)
    out.save(dst, "JPEG", quality=quality, optimize=True)


def verify(path: Path) -> None:
    raw = path.read_bytes()
    # Metadata lives in the marker segments before start-of-scan; scanning the
    # entropy-coded data would give random false hits on short strings.
    sos = raw.find(b"\xff\xda")
    raw = raw[:sos] if sos > 0 else raw
    with Image.open(path) as im:
        problems = []
        if len(im.getexif()):
            problems.append("EXIF")
        problems += [k for k in ("xmp", "icc_profile", "comment", "exif") if k in im.info]
        problems += [b.decode() for b in SUSPECT_BYTES if b in raw]
    if problems:
        sys.exit(f"{path}: metadata survived: {problems}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", type=Path)
    ap.add_argument("--long-edge", type=int, default=1600)
    ap.add_argument("--quality", type=int, default=90)
    args = ap.parse_args()

    args.dst.mkdir(parents=True, exist_ok=True)
    inputs = sorted(p for p in args.src.iterdir()
                    if p.suffix.lower() in (".jpg", ".jpeg", ".png"))
    if not inputs:
        sys.exit(f"no photos in {args.src}")
    total = 0
    for src in inputs:
        dst = args.dst / (src.stem + ".jpg")
        clean(src, dst, args.long_edge, args.quality)
        verify(dst)
        size = dst.stat().st_size
        total += size
        with Image.open(dst) as im:
            print(f"{src.name:>14} -> {dst.name}  {im.width}x{im.height}  {size // 1024} KB")
    print(f"{len(inputs)} photos, {total / 1048576:.1f} MB total, metadata check passed")


if __name__ == "__main__":
    main()
