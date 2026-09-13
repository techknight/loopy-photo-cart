#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Reference encoder for the Loopy Photo Cart pack format, and the ROM patcher
# that appends a pack to a template (docs/pack-format.md).
#
# This module has no dependencies beyond the standard library, so the golden
# files for the web app's tests can be regenerated anywhere. Image conversion
# lives in lpcimage.py.

import struct
import zlib
from dataclasses import dataclass, field

ROM_BASE = 0x0E000000
ROM_PAD_BLOCK = 4096

DESC_OFFSET = 0x20
DESC_MAGIC = b"LPCT"
DESC_VERSION = 1

PACK_MAGIC = b"LPCP"
PACK_VERSION = 1

HEADER_SIZE = 40
IMAGE_REF_SIZE = 20
PHOTO_SIZE = 28
PAGE_SIZE = 60

IMAGE_W = 256
IMAGE_H = 240
PALETTE_SIZE = 256

MAX_PHOTOS = 54
GRID_COLS = 3
GRID_ROWS = 3
CELLS_PER_PAGE = GRID_COLS * GRID_ROWS

ORIENT_LANDSCAPE = 0
ORIENT_PORTRAIT = 1  # stored rotated 90 degrees clockwise

FLAG_PRINT_PALETTES = 0x0001

# Reserved UI slots 248..255, as 5-bit (r, g, b). Must match
# docs/pack-format.md and cart/src/ui.c.
UI_FIRST = 248
UI_COLOURS = [
    (0, 0, 0),     # black
    (3, 4, 8),     # background
    (6, 7, 13),    # panel
    (12, 12, 15),  # dim
    (22, 22, 24),  # grey
    (31, 31, 31),  # white
    (31, 14, 20),  # accent
    (31, 27, 8),   # highlight
]


def rgb555(r, g, b):
    return (r << 10) | (g << 5) | b


UI_PALETTE = [rgb555(*c) for c in UI_COLOURS]


def align4(n):
    return (n + 3) & ~3


@dataclass
class Image:
    palette: list    # 256 RGB555 values; slot 0 black, 248..255 UI_PALETTE
    pixels: bytes    # IMAGE_W * IMAGE_H palette indices, row 0 at the top


@dataclass
class Photo:
    image: Image
    orientation: int = ORIENT_LANDSCAPE
    print_palette: list = None


@dataclass
class Page:
    image: Image
    first_photo: int
    cells: list = field(default_factory=list)  # [(x, y, w, h)], 1..9


class PackError(ValueError):
    pass


def _check_palette(palette, what):
    if len(palette) != PALETTE_SIZE:
        raise PackError(f"{what}: palette has {len(palette)} entries, not 256")
    if any(not 0 <= c <= 0x7FFF for c in palette):
        raise PackError(f"{what}: palette entry out of RGB555 range")
    if palette[0] != 0:
        raise PackError(f"{what}: slot 0 must be black (it is also the backdrop)")
    if list(palette[UI_FIRST:]) != UI_PALETTE:
        raise PackError(f"{what}: slots 248-255 must be the reserved UI colours")


def _check_image(image, what):
    _check_palette(image.palette, what)
    if len(image.pixels) != IMAGE_W * IMAGE_H:
        raise PackError(f"{what}: {len(image.pixels)} pixels, not {IMAGE_W * IMAGE_H}")


def encode_pack(photos, pages=(), meta=None):
    """Build a pack. Returns bytes."""
    photos = list(photos)
    pages = list(pages)
    meta = dict(meta or {})

    if len(photos) > MAX_PHOTOS:
        raise PackError(f"{len(photos)} photos; at most {MAX_PHOTOS}")
    if pages and len(pages) != -(-len(photos) // CELLS_PER_PAGE):
        raise PackError("page count must be 0 or ceil(photos / 9)")
    for i, p in enumerate(photos):
        _check_image(p.image, f"photo {i}")
        if p.orientation not in (ORIENT_LANDSCAPE, ORIENT_PORTRAIT):
            raise PackError(f"photo {i}: bad orientation {p.orientation}")
        if p.print_palette is not None:
            _check_palette(p.print_palette, f"photo {i} print palette")
    for i, pg in enumerate(pages):
        _check_image(pg.image, f"page {i}")
        if not 1 <= len(pg.cells) <= CELLS_PER_PAGE:
            raise PackError(f"page {i}: {len(pg.cells)} cells")
        if pg.first_photo + len(pg.cells) > len(photos):
            raise PackError(f"page {i}: cells run past the last photo")

    meta_bytes = b""
    for key, value in meta.items():
        if not key or "=" in key or "\0" in key + str(value):
            raise PackError(f"bad meta key {key!r}")
        meta_bytes += f"{key}={value}".encode("utf-8") + b"\0"
    meta_bytes += b"\0"

    photo_table = HEADER_SIZE
    page_table = photo_table + PHOTO_SIZE * len(photos)
    meta_off = page_table + PAGE_SIZE * len(pages)
    tables_end = align4(meta_off + len(meta_bytes))

    blobs = bytearray()

    def put(data):
        off = tables_end + len(blobs)
        blobs.extend(data)
        blobs.extend(b"\0" * (align4(len(blobs)) - len(blobs)))
        return off

    def put_palette(palette):
        return put(struct.pack(">256H", *palette))

    def put_image(image):
        palette_off = put_palette(image.palette)
        pixels_off = put(image.pixels)
        return struct.pack(">HHBBHIII", IMAGE_W, IMAGE_H, 0, 0, 0,
                           palette_off, pixels_off, len(image.pixels))

    tables = bytearray()
    for p in photos:
        ref = put_image(p.image)
        print_off = put_palette(p.print_palette) if p.print_palette is not None else 0
        tables += ref + struct.pack(">IBBH", print_off, p.orientation, 0, 0)
    for pg in pages:
        ref = put_image(pg.image)
        cells = list(pg.cells) + [(0, 0, 0, 0)] * (CELLS_PER_PAGE - len(pg.cells))
        tables += ref + struct.pack(">HBB", pg.first_photo, len(pg.cells), 0)
        for c in cells:
            tables += struct.pack("4B", *c)
    tables += meta_bytes
    tables += b"\0" * (tables_end - HEADER_SIZE - len(tables))

    flags = FLAG_PRINT_PALETTES if any(p.print_palette is not None for p in photos) else 0
    total = tables_end + len(blobs)
    header = struct.pack(">4sHHIIHHIIIIBBH", PACK_MAGIC, PACK_VERSION, flags,
                         total, 0, len(photos), len(pages), tables_end,
                         photo_table, page_table, meta_off,
                         GRID_COLS, GRID_ROWS, 0)

    pack = bytearray(header + tables + blobs)
    assert len(header) == HEADER_SIZE and len(pack) == total
    struct.pack_into(">I", pack, 12, zlib.crc32(pack[16:tables_end]) & 0xFFFFFFFF)
    return bytes(pack)


@dataclass
class Descriptor:
    desc_version: int
    pack_version: int
    pack_base: int
    rom_limit: int
    build_id: str


def read_descriptor(template):
    if len(template) < DESC_OFFSET + 32:
        raise PackError("template is too short to hold a descriptor")
    magic, dver, pver, base, limit = struct.unpack_from(">4sHHII", template, DESC_OFFSET)
    if magic != DESC_MAGIC:
        raise PackError("not a Loopy Photo Cart template (no LPCT descriptor)")
    if dver != DESC_VERSION:
        raise PackError(f"unsupported template descriptor version {dver}")
    build_id = template[DESC_OFFSET + 16:DESC_OFFSET + 32].split(b"\0")[0].decode("ascii")
    return Descriptor(dver, pver, base, limit, build_id)


def loopy_checksum(rom):
    """The cartridge header checksum, as tools/fixrom.py computes it."""
    first, last = struct.unpack_from(">II", rom, 0)
    start, end = first - ROM_BASE, last + 2 - ROM_BASE
    total = 0
    for (word,) in struct.iter_unpack(">H", rom[start:end]):
        total = (total + word) & 0xFFFFFFFF
    return total


def patch_rom(template, pack):
    """Append `pack` to `template` and fix the header. Returns bytes."""
    d = read_descriptor(template)
    magic, version, _flags, total = struct.unpack_from(">4sHHI", pack, 0)
    if magic != PACK_MAGIC:
        raise PackError("not a photo pack")
    if version != d.pack_version:
        raise PackError(f"template reads pack version {d.pack_version}, pack is {version}")
    if total != len(pack):
        raise PackError("pack total_size does not match its length")

    pack_offset = d.pack_base - ROM_BASE
    if len(template) > pack_offset:
        raise PackError(f"template ({len(template)} bytes) overlaps the pack region")

    rom = bytearray(template) + b"\xFF" * (pack_offset - len(template)) + pack
    if len(rom) % 2:
        rom += b"\xFF"
    if ROM_BASE + len(rom) > d.rom_limit:
        raise PackError(f"ROM would be {len(rom)} bytes; the cartridge holds "
                        f"{d.rom_limit - ROM_BASE}")

    struct.pack_into(">I", rom, 4, ROM_BASE + len(rom) - 2)
    struct.pack_into(">I", rom, 8, loopy_checksum(rom))
    rom += b"\xFF" * (-len(rom) % ROM_PAD_BLOCK)
    return bytes(rom)
