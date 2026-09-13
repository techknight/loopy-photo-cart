// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Grid pages, laid out exactly like cart/tools/lpcimage.py's render_pages.

import type { RgbImage } from "./image.ts";
import { newRgb, paste } from "./image.ts";
import { HOME_VIDEO, PUBLIC_PIXEL } from "./fonts.gen.ts";
import type { Cell, PackPage } from "./pack.ts";
import {
  CELLS_PER_PAGE,
  IMAGE_H,
  IMAGE_W,
  THUMB_H,
  THUMB_W,
  UI_ACCENT,
  UI_BG,
  UI_GREY,
  UI_WHITE,
} from "./palette.ts";
import { quantize } from "./quantize.ts";
import { drawHint, drawText } from "./text.ts";

export const CELL_XS = [16, 96, 176] as const;
export const CELL_YS = [34, 100, 166] as const;
export const HEADER_X = 16;
export const HEADER_Y = 14;
export const HEADER_RIGHT = 240;
export const HEADER_TITLE = "PHOTOS";
export const CONTROLS_X = 100;
export const CONTROLS_YS = [13, 22] as const;
export const CONTROLS = ["A: View", "D: Show Help"] as const;

/** Draw the page header into an indexed page. */
export function drawHeader(pixels: Uint8Array, page: number, pageCount: number): void {
  drawText(pixels, IMAGE_W, IMAGE_H, HOME_VIDEO, HEADER_X, HEADER_Y, HEADER_TITLE, UI_ACCENT);
  CONTROLS.forEach((line, i) =>
    drawHint(pixels, IMAGE_W, IMAGE_H, PUBLIC_PIXEL, CONTROLS_X, CONTROLS_YS[i]!, line, UI_WHITE),
  );
  const counter = `${page + 1}/${pageCount}`;
  drawText(
    pixels,
    IMAGE_W,
    IMAGE_H,
    HOME_VIDEO,
    HEADER_RIGHT - HOME_VIDEO.cellW * counter.length,
    HEADER_Y,
    counter,
    UI_GREY,
  );
}

export function cellsFor(count: number): Cell[] {
  const cells: Cell[] = [];
  for (let i = 0; i < count; i++) cells.push([CELL_XS[i % 3]!, CELL_YS[Math.floor(i / 3)]!, THUMB_W, THUMB_H]);
  return cells;
}

/** 64x60 thumbnails in photo order -> grid pages (not dithered). */
export function renderPages(thumbs: readonly RgbImage[]): PackPage[] {
  const pageCount = Math.ceil(thumbs.length / CELLS_PER_PAGE);
  const pages: PackPage[] = [];
  for (let p = 0; p < pageCount; p++) {
    const chunk = thumbs.slice(p * CELLS_PER_PAGE, (p + 1) * CELLS_PER_PAGE);
    const cells = cellsFor(chunk.length);
    const canvas = newRgb(IMAGE_W, IMAGE_H);
    chunk.forEach((thumb, i) => paste(canvas, thumb, cells[i]![0], cells[i]![1]));
    const image = quantize(canvas, false);

    const inside = new Uint8Array(IMAGE_W * IMAGE_H);
    for (const [x, y, w, h] of cells) {
      for (let row = y; row < y + h; row++) inside.fill(1, row * IMAGE_W + x, row * IMAGE_W + x + w);
    }
    for (let i = 0; i < inside.length; i++) if (!inside[i]) image.pixels[i] = UI_BG;
    drawHeader(image.pixels, p, pageCount);

    pages.push({ image: { palette: image.palette, pixels: image.pixels }, firstPhoto: p * CELLS_PER_PAGE, cells });
  }
  return pages;
}
