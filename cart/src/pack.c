//
// Copyright(C) 2026 Derek Quenneville
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// Photo pack discovery and validation. The layout and every rule checked
// here are specified in docs/pack-format.md; the structs below are that
// document's tables, big-endian like the SH-1.
//

#include <stddef.h>

#include "lpc.h"
#include "pack.h"

// The cartridge header's second longword: the address of the last word the
// checksum covers, so the data ends two bytes after it (tools/fixrom.py).
#define ROM_HEADER_ROMLAST (*(const uint32_t *) (LPC_ROM_BASE + 4))

struct pack_header {
	char     magic[4];
	uint16_t version;
	uint16_t flags;
	uint32_t total_size;
	uint32_t header_crc32;
	uint16_t photo_count;
	uint16_t page_count;
	uint32_t tables_end;
	uint32_t photo_table;
	uint32_t page_table;
	uint32_t meta;
	uint8_t  grid_cols;
	uint8_t  grid_rows;
	uint16_t reserved;
};

struct pack_image {
	uint16_t width;
	uint16_t height;
	uint8_t  codec;
	uint8_t  reserved0;
	uint16_t reserved1;
	uint32_t palette;
	uint32_t pixels;
	uint32_t length;
};

struct pack_photo {
	struct pack_image image;
	uint32_t print_palette;
	uint8_t  orientation;
	uint8_t  reserved0;
	uint16_t reserved1;
};

struct pack_cell {
	uint8_t x, y, w, h;
};

struct pack_page {
	struct pack_image image;
	uint16_t first_photo;
	uint8_t  cell_count;
	uint8_t  reserved;
	struct pack_cell cells[LPC_GRID_COLS * LPC_GRID_ROWS];
};

_Static_assert(sizeof(struct pack_header) == 40, "PackHeader is 40 bytes");
_Static_assert(offsetof(struct pack_header, photo_count) == 16, "PackHeader layout");
_Static_assert(offsetof(struct pack_header, grid_cols) == 36, "PackHeader layout");
_Static_assert(sizeof(struct pack_image) == 20, "ImageRef is 20 bytes");
_Static_assert(sizeof(struct pack_photo) == 28, "PhotoEntry is 28 bytes");
_Static_assert(sizeof(struct pack_page) == 60, "PageEntry is 60 bytes");

#define HEADER_SIZE   ((uint32_t) sizeof(struct pack_header))
#define PALETTE_BYTES 512u
#define IMAGE_BYTES   ((uint32_t) LPC_IMAGE_W * LPC_IMAGE_H)

static const struct pack_header *pack;

// --------------------------------------------------------------------------
// CRC-32 (IEEE 802.3, the zlib one)
// --------------------------------------------------------------------------

static uint32_t crc_table[256];

static void CrcInit(void)
{
	uint32_t n, c;
	int k;

	for (n = 0; n < 256; ++n) {
		c = n;
		for (k = 0; k < 8; ++k)
			c = (c & 1) ? 0xEDB88320ul ^ (c >> 1) : c >> 1;
		crc_table[n] = c;
	}
}

static uint32_t Crc32(const uint8_t *p, uint32_t len)
{
	uint32_t c = 0xFFFFFFFFul;

	while (len--)
		c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFul;
}

// --------------------------------------------------------------------------
// Validation
// --------------------------------------------------------------------------

// [off, off + len) lies inside [lo, hi) and starts word-aligned. Written so
// no sum can wrap.
static int Span(uint32_t off, uint32_t len, uint32_t lo, uint32_t hi)
{
	return (off & 3u) == 0 && off >= lo && off <= hi && len <= hi - off;
}

static int ImageOk(const struct pack_image *im, const struct pack_header *h)
{
	return im->width == LPC_IMAGE_W && im->height == LPC_IMAGE_H &&
	       im->codec == 0 && im->length == IMAGE_BYTES &&
	       Span(im->palette, PALETTE_BYTES, h->tables_end, h->total_size) &&
	       Span(im->pixels, im->length, h->tables_end, h->total_size);
}

static enum lpc_pack_status CheckTables(const struct pack_header *h)
{
	const struct pack_photo *photos;
	const struct pack_page *pages;
	unsigned i, max_pages;

	if (h->photo_count > LPC_MAX_PHOTOS)
		return LPC_PACK_BAD_TABLES;
	max_pages = (h->photo_count + LPC_GRID_COLS * LPC_GRID_ROWS - 1) /
	            (LPC_GRID_COLS * LPC_GRID_ROWS);
	if (h->page_count != 0 && h->page_count != max_pages)
		return LPC_PACK_BAD_TABLES;
	if (h->grid_cols != LPC_GRID_COLS || h->grid_rows != LPC_GRID_ROWS)
		return LPC_PACK_BAD_TABLES;

	if (!Span(h->photo_table, h->photo_count * (uint32_t) sizeof *photos,
	          HEADER_SIZE, h->tables_end) ||
	    !Span(h->page_table, h->page_count * (uint32_t) sizeof *pages,
	          HEADER_SIZE, h->tables_end) ||
	    (h->meta != 0 && !Span(h->meta, 1, HEADER_SIZE, h->tables_end)))
		return LPC_PACK_BAD_TABLES;

	photos = (const struct pack_photo *) (LPC_PACK_BASE + h->photo_table);
	for (i = 0; i < h->photo_count; ++i) {
		const struct pack_photo *p = &photos[i];

		if (!ImageOk(&p->image, h) ||
		    p->orientation > LPC_ORIENT_PORTRAIT ||
		    (p->print_palette != 0 &&
		     !Span(p->print_palette, PALETTE_BYTES, h->tables_end,
		           h->total_size)))
			return LPC_PACK_BAD_TABLES;
	}

	pages = (const struct pack_page *) (LPC_PACK_BASE + h->page_table);
	for (i = 0; i < h->page_count; ++i) {
		const struct pack_page *pg = &pages[i];

		if (!ImageOk(&pg->image, h) || pg->cell_count == 0 ||
		    pg->cell_count > LPC_GRID_COLS * LPC_GRID_ROWS ||
		    (uint32_t) pg->first_photo + pg->cell_count > h->photo_count)
			return LPC_PACK_BAD_TABLES;
	}

	return LPC_PACK_OK;
}

enum lpc_pack_status LPC_PackOpen(void)
{
	const struct pack_header *h = (const struct pack_header *) LPC_PACK_BASE;
	uint32_t rom_end = ROM_HEADER_ROMLAST + 2;
	enum lpc_pack_status status;

	pack = NULL;

	// A bare template ends well below the pack region. Past its end the
	// cartridge reads back as open bus or a mirror of the start of the ROM,
	// neither of which may be mistaken for photos.
	if (rom_end < LPC_PACK_BASE + 16)
		return LPC_PACK_ABSENT;

	if (h->magic[0] != 'L' || h->magic[1] != 'P' ||
	    h->magic[2] != 'C' || h->magic[3] != 'P')
		return LPC_PACK_BAD_MAGIC;

	if (h->version != LPC_PACK_VERSION)
		return LPC_PACK_BAD_VERSION;

	if (rom_end < LPC_PACK_BASE + HEADER_SIZE ||
	    h->total_size < HEADER_SIZE ||
	    h->total_size > LPC_ROM_LIMIT - LPC_PACK_BASE ||
	    h->total_size > rom_end - LPC_PACK_BASE)
		return LPC_PACK_BAD_SIZE;

	if ((h->tables_end & 3u) != 0 || h->tables_end < HEADER_SIZE ||
	    h->tables_end > h->total_size)
		return LPC_PACK_BAD_TABLES;

	// Only the header and tables: a CRC over the images would take seconds.
	CrcInit();
	if (Crc32((const uint8_t *) h + 16, h->tables_end - 16) != h->header_crc32)
		return LPC_PACK_BAD_CRC;

	status = CheckTables(h);
	if (status == LPC_PACK_OK)
		pack = h;
	return status;
}

unsigned LPC_PackPhotoCount(void)
{
	return pack ? pack->photo_count : 0;
}

void LPC_PackPhoto(unsigned index, struct lpc_photo *out)
{
	const struct pack_photo *p = &((const struct pack_photo *)
	    (LPC_PACK_BASE + pack->photo_table))[index];

	out->pixels = (const uint8_t *) (LPC_PACK_BASE + p->image.pixels);
	out->palette = (const uint16_t *) (LPC_PACK_BASE + p->image.palette);
	out->print_palette = p->print_palette
	    ? (const uint16_t *) (LPC_PACK_BASE + p->print_palette)
	    : out->palette;
	out->orientation = p->orientation;
}

const char *LPC_PackStatusText(enum lpc_pack_status status)
{
	switch (status) {
	// At most 18 characters: one line of Home Video Font on screen.
	case LPC_PACK_OK:          return "PHOTOS FOUND";
	case LPC_PACK_ABSENT:      return "NO PHOTO PACK";
	case LPC_PACK_BAD_MAGIC:   return "UNKNOWN PACK";
	case LPC_PACK_BAD_VERSION: return "NEWER PACK FORMAT";
	case LPC_PACK_BAD_SIZE:    return "PACK IS DAMAGED";
	case LPC_PACK_BAD_CRC:     return "PACK CHECK FAILED";
	case LPC_PACK_BAD_TABLES:  return "PACK TABLES BAD";
	}
	return "UNKNOWN";
}
