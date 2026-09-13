// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Bitmap text into an indexed image, from the glyph tables in fonts.gen.ts.
// Reproduces cart/tools/lpcimage.py's text pixel for pixel.

export interface BitmapFont {
  cellW: number;
  rows: number;
  first: number;
  last: number;
  /** `rows` entries per glyph; leftmost pixel of the cell in bit cellW - 1. */
  glyphs: Uint16Array;
}

function glyph(
  pixels: Uint8Array,
  width: number,
  height: number,
  font: BitmapFont,
  x: number,
  y: number,
  code: number,
  colour: number,
): void {
  if (code < font.first || code > font.last) return;
  const base = (code - font.first) * font.rows;
  for (let r = 0; r < font.rows; r++) {
    const bits = font.glyphs[base + r]!;
    if (!bits) continue;
    const py = y + r;
    if (py < 0 || py >= height) continue;
    for (let c = 0; c < font.cellW; c++) {
      if (!(bits & (1 << (font.cellW - 1 - c)))) continue;
      const px = x + c;
      if (px >= 0 && px < width) pixels[py * width + px] = colour;
    }
  }
}

/** Monospaced text with its ink's top-left at (x, y). */
export function drawText(
  pixels: Uint8Array,
  width: number,
  height: number,
  font: BitmapFont,
  x: number,
  y: number,
  text: string,
  colour: number,
): void {
  for (let i = 0; i < text.length; i++) {
    glyph(pixels, width, height, font, x + i * font.cellW, y, text.charCodeAt(i), colour);
  }
}

/** Like drawText, but a space after a colon is half a cell ("A: View"). */
export function drawHint(
  pixels: Uint8Array,
  width: number,
  height: number,
  font: BitmapFont,
  x: number,
  y: number,
  text: string,
  colour: number,
): void {
  let prev = "";
  for (const ch of text) {
    if (ch === " " && prev === ":") {
      x += Math.floor(font.cellW / 2);
    } else {
      if (ch !== " ") glyph(pixels, width, height, font, x, y, ch.charCodeAt(0), colour);
      x += font.cellW;
    }
    prev = ch;
  }
}
