// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// From photo pixels and crop choices to a finished cartridge ROM.

import type { CropState } from "./crop.ts";
import { renderUpright, thumbnail } from "./crop.ts";
import type { PixelSource, RgbImage } from "./image.ts";
import { rotateClockwise } from "./image.ts";
import type { PackPhoto } from "./pack.ts";
import { encodePack, PackError } from "./pack.ts";
import { renderPages } from "./pages.ts";
import { MAX_PHOTOS, ORIENT_LANDSCAPE, ORIENT_PORTRAIT, THUMB_H, THUMB_W } from "./palette.ts";
import { quantize } from "./quantize.ts";
import { buildRom } from "./rompatch.ts";

export const TOOL_NAME = "loopy-photo-cart-web 0.1";

export interface RenderedPhoto {
  photo: PackPhoto;
  /** 64x60, for the grid page. */
  thumb: RgbImage;
}

export function renderPhoto(src: PixelSource, crop: CropState, dither: boolean): RenderedPhoto {
  const upright = renderUpright(src, crop);
  const portrait = crop.orientation === "portrait";
  const stored = portrait ? rotateClockwise(upright) : upright;
  const q = quantize(stored, dither);
  return {
    photo: {
      image: { palette: q.palette, pixels: q.pixels },
      orientation: portrait ? ORIENT_PORTRAIT : ORIENT_LANDSCAPE,
    },
    thumb: thumbnail(upright, THUMB_W, THUMB_H),
  };
}

export function buildPackFromPhotos(
  rendered: readonly RenderedPhoto[],
  meta: Readonly<Record<string, string>> = {},
): Uint8Array {
  if (rendered.length === 0) throw new PackError("Add at least one photo.");
  if (rendered.length > MAX_PHOTOS) throw new PackError(`A cartridge holds at most ${MAX_PHOTOS} photos.`);
  const pages = renderPages(rendered.map((r) => r.thumb));
  return encodePack(rendered.map((r) => r.photo), pages, { title: "Photos", tool: TOOL_NAME, ...meta });
}

export function buildCartridge(
  template: Uint8Array,
  rendered: readonly RenderedPhoto[],
  meta: Readonly<Record<string, string>> = {},
): Uint8Array {
  return buildRom(template, buildPackFromPhotos(rendered, meta));
}
