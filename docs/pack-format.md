# Photo pack format, version 1

The photo pack is the data the web app appends to the template ROM. The
cartridge program reads it in place from ROM.

Three implementations must agree byte for byte:
- the cart reader, `cart/src/pack.c`;
- the Python reference encoder, `cart/tools/lpcpack.py`;
- the web app's TypeScript encoder.

`web/test/fixtures` holds golden files that the TypeScript tests are checked
against.

## Placement in the ROM

| Field | Value |
|---|---|
| Cartridge base | `0x0E000000` |
| Pack base | Read from the template descriptor (currently `0x0E040000`, ROM offset `0x40000`) |
| Cartridge limit | Read from the descriptor (currently `0x0E400000`: the Floopy Drive's 4 MB) |

To build a ROM:

1. **Read the template descriptor** at ROM offset `0x20`. The magic must be
   `LPCT` and the descriptor version 1. The template's pack version must equal
   the pack's version.
2. **Refuse an overlapping template:** if it is longer than the pack offset,
   stop.
3. **Lay out the file:** pad the template with `0xFF` up to the pack offset,
   append the pack, then add one `0xFF` if the length is odd.
4. **Check the size:** refuse if `base + length` exceeds the cartridge limit.
5. **Patch the header:**
   - `romlast` (big-endian u32 at offset 4) is `0x0E000000 + length - 2`;
   - the checksum (u32 at offset 8) is the 32-bit wrapping sum of big-endian
     u16 words from `romfirst` (header offset 0) up to `romlast + 2`. This is
     the same calculation as `cart/tools/fixrom.py`.
6. **Pad the file** with `0xFF` to a multiple of 4096 bytes. This padding is
   outside the checksummed range.

## Conventions

- **Byte order:** big-endian throughout, the SH-1's native order.
- **Offsets:** every offset is a u32 counted from the pack base, and is a
  multiple of 4. The SH-1 raises an address error on a 16-bit or 32-bit read
  at an odd address, and 4 keeps copies fast.
- **Colours:** RGB555, as `r << 10 | g << 5 | b`, with 5 bits per channel.
- **Images:** 256×240, one palette index per pixel, row 0 at the top.
- **Pixel index 0:** transparent on the Loopy's bitmap layer, so it shows the
  backdrop. The backdrop also fills the TV raster outside the 256-pixel-wide
  picture. The cart sets the backdrop to palette entry 0, so **entry 0 must
  be black** in every palette: the border stays black, and index 0 prints the
  same colour it shows. Photo colours therefore use slots 1–247.

## Layout

```
PACK_BASE + 0            PackHeader (40 bytes)
          + photo_table  PhotoEntry[photo_count]
          + page_table   PageEntry[page_count]
          + meta         key=value strings
          + tables_end   (aligned to 4) images: palettes and pixel data
          + total_size
```

Everything from the pack base up to `tables_end` is the **tables region**,
which `header_crc32` covers (all of it after the first 16 bytes). Image data
follows it and isn't covered. A CRC over ~3.7 MB would take seconds at boot on
the SH-1, and the web app verifies the finished ROM.

### PackHeader (40 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | char[4] | `magic` | `LPCP` |
| 4 | u16 | `version` | 1 |
| 6 | u16 | `flags` | bit 0: at least one photo has a print palette. Other bits 0. |
| 8 | u32 | `total_size` | Pack length in bytes |
| 12 | u32 | `header_crc32` | CRC-32 (IEEE, as zlib) of bytes `[16, tables_end)` |
| 16 | u16 | `photo_count` | 0–54 |
| 18 | u16 | `page_count` | 0 (no grid), or `ceil(photo_count / 9)` |
| 20 | u32 | `tables_end` | Aligned; the first image data starts here |
| 24 | u32 | `photo_table` | Offset of `PhotoEntry[0]` |
| 28 | u32 | `page_table` | Offset of `PageEntry[0]` |
| 32 | u32 | `meta` | Offset of the meta strings, or 0 for none |
| 36 | u8 | `grid_cols` | 3 |
| 37 | u8 | `grid_rows` | 3 |
| 38 | u16 | reserved | 0 |

### ImageRef (20 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | u16 | `width` | 256 |
| 2 | u16 | `height` | 240 |
| 4 | u8 | `codec` | 0 = raw. No other value is defined. |
| 5 | u8 | reserved | 0 |
| 6 | u16 | reserved | 0 |
| 8 | u32 | `palette` | Offset of 256 × u16 colours (512 bytes) |
| 12 | u32 | `pixels` | Offset of the pixel data |
| 16 | u32 | `length` | Pixel data length: `width × height` for codec 0 |

### PhotoEntry (28 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | ImageRef | `image` | The photo, for both screen and printer |
| 20 | u32 | `print_palette` | Offset of a 512-byte palette used only when printing, or 0 to print with `image.palette` |
| 24 | u8 | `orientation` | 0 = landscape. 1 = portrait, stored **rotated 90° clockwise** so it fills the sticker lengthwise. |
| 25 | u8 | reserved | 0 |
| 26 | u16 | reserved | 0 |

For a portrait photo, the upright picture is 240 wide by 256 tall, and stored
pixel `(x, y)` is upright pixel `(y, 255 − x)`. The cart turns it back upright
on screen and prints the stored buffer as-is.

### PageEntry (60 bytes)

A pre-rendered grid page: thumbnails, header and page number composed by the
web app.

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | ImageRef | `image` | The whole 256×240 page |
| 20 | u16 | `first_photo` | Index of the photo in cell 0 |
| 22 | u8 | `cell_count` | 1–9 |
| 23 | u8 | reserved | 0 |
| 24 | Cell[9] | `cells` | `u8 x, u8 y, u8 w, u8 h` per cell: where the cursor frame goes. Unused cells are 0. |

Cell `i` shows photo `first_photo + i`, and cells run left to right, top to
bottom.

### Meta strings

A sequence of NUL-terminated UTF-8 strings of the form `key=value`, ended by an
empty string. Keys are lowercase ASCII.

| Key | Meaning |
|---|---|
| `title` | Pack title, as the user typed it |
| `tool` | Encoder name and version |
| `created` | ISO 8601 date |

The cart ignores keys it doesn't know.

## Reserved UI palette slots

Every palette, whether for a photo, a page or print, carries black in slot 0
(see Conventions) and these colours in slots 248–255. The cart can then draw its cursor, dialogs and on-screen text
over any image without changing the palette.

Photo pixels must never use slots 248–255. When dithering is allowed to reach
for them, the UI colours show up as speckles (the highlight yellow on orange
fur, for example). Grid page images may use them for their own UI elements.

| Slot | Name | RGB555 (r, g, b) |
|---|---|---|
| 248 | black | 0, 0, 0 |
| 249 | background | 3, 4, 8 |
| 250 | panel | 6, 7, 13 |
| 251 | dim | 12, 12, 15 |
| 252 | grey | 22, 22, 24 |
| 253 | white | 31, 31, 31 |
| 254 | accent | 31, 14, 20 |
| 255 | highlight | 31, 27, 8 |

## Validation the cart performs

If any check fails, the cart shows the no-photos screen with the reason.

- **Header:**
  - The ROM header's `romlast + 2` must reach at least the pack base plus 16
    bytes; otherwise there is simply no pack.
  - `magic` and `version` must be right, and the header must fit in the ROM.
  - `total_size` must stay within both the ROM and the 4 MB limit.
  - `tables_end` must be aligned and lie within `[40, total_size]`.
- **CRC:** `header_crc32` must match.
- **Counts:**
  - At most 54 photos.
  - `page_count` must be 0 or `ceil(photo_count / 9)`.
  - The grid must be 3×3.
- **Table offsets:** `photo_table`, `page_table` and a non-zero `meta` must lie
  aligned inside the tables region.
- **Every ImageRef:** 256×240, codec 0, and length 57,600. Its palette and
  pixels must lie aligned inside `[tables_end, total_size)`.
- **Every photo:** `orientation` must be 0 or 1, and a non-zero
  `print_palette` must lie inside the image region.
- **Every page:** `cell_count` must be 1–9, and
  `first_photo + cell_count ≤ photo_count`.

## Size budget

A 54-photo pack with print palettes and 6 grid pages takes 3,514,368 bytes of
images plus under 4 KB of tables. That fits below the 4 MB limit with the
256 KB code region (`docs/PLAN.md` §8).
