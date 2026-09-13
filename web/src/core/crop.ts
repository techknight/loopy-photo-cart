// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Crop state and rendering. Every photo fills the sticker by default: the crop
// box has the sticker's shape, can be moved and zoomed, and can never extend
// past the photo. "Fit" shows the whole photo over a blurred copy of itself
// instead, so nothing prints blank.

import type { PixelSource, RgbImage } from "./image.ts";
import { newRgb, paste } from "./image.ts";
import { IMAGE_H, IMAGE_W } from "./palette.ts";
import { boxBlur, resampleRect } from "./resample.ts";

/** Placeholder until the hardware print test measures the sticker
 *  (docs/PLAN.md section 8). Must match cart/tools/lpcimage.py. */
export const STICKER_ASPECT = 4 / 3;
export const MAX_ZOOM = 6;
/** Below this share of the photo kept, the UI flags the crop for review. */
export const LOW_KEPT_FRACTION = 0.6;

export type Orientation = "landscape" | "portrait";
export type FitMode = "fill" | "fit";

export interface CropState {
  orientation: Orientation;
  mode: FitMode;
  /** 1 = the largest sticker-shaped box that fits the photo. */
  zoom: number;
  /** Centre of the crop box, in source pixels. */
  cx: number;
  cy: number;
}

export interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}

export function stickerAspect(o: Orientation): number {
  return o === "portrait" ? 1 / STICKER_ASPECT : STICKER_ASPECT;
}

/** Upright size of the stored image: 256x240, or 240x256 for portrait. */
export function uprightSize(o: Orientation): { w: number; h: number } {
  return o === "portrait" ? { w: IMAGE_H, h: IMAGE_W } : { w: IMAGE_W, h: IMAGE_H };
}

export function coverSize(imgW: number, imgH: number, aspect: number): { w: number; h: number } {
  return imgW / imgH > aspect ? { w: imgH * aspect, h: imgH } : { w: imgW, h: imgW / aspect };
}

function clamp(v: number, lo: number, hi: number): number {
  return Math.min(hi, Math.max(lo, v));
}

export function cropRect(imgW: number, imgH: number, s: CropState): Rect {
  const base = coverSize(imgW, imgH, stickerAspect(s.orientation));
  const zoom = clamp(s.zoom, 1, MAX_ZOOM);
  const w = base.w / zoom;
  const h = base.h / zoom;
  const x = clamp(s.cx - w / 2, 0, imgW - w);
  const y = clamp(s.cy - h / 2, 0, imgH - h);
  return { x, y, w, h };
}

/** The same state with zoom in range and the centre pulled back so the box
 *  stays inside the photo. */
export function clampCrop(imgW: number, imgH: number, s: CropState): CropState {
  const r = cropRect(imgW, imgH, s);
  return { ...s, zoom: clamp(s.zoom, 1, MAX_ZOOM), cx: r.x + r.w / 2, cy: r.y + r.h / 2 };
}

export function defaultCrop(
  imgW: number,
  imgH: number,
  focus?: { x: number; y: number },
): CropState {
  const orientation: Orientation = imgH > imgW ? "portrait" : "landscape";
  return clampCrop(imgW, imgH, {
    orientation,
    mode: "fill",
    zoom: 1,
    cx: focus?.x ?? imgW / 2,
    cy: focus?.y ?? imgH / 2,
  });
}

export function keptFraction(imgW: number, imgH: number, s: CropState): number {
  if (s.mode === "fit") return 1;
  const r = cropRect(imgW, imgH, s);
  return (r.w * r.h) / (imgW * imgH);
}

/** Render the upright sticker image (256x240 or 240x256) for `s`. */
export function renderUpright(src: PixelSource, s: CropState): RgbImage {
  const { w: tw, h: th } = uprightSize(s.orientation);

  if (s.mode === "fill") {
    const r = cropRect(src.width, src.height, s);
    return resampleRect(src, r.x, r.y, r.w, r.h, tw, th);
  }

  // Fit: the whole photo, centred, over a blurred and dimmed fill of itself.
  const cover = coverSize(src.width, src.height, tw / th);
  const bg = boxBlur(
    resampleRect(src, (src.width - cover.w) / 2, (src.height - cover.h) / 2, cover.w, cover.h, tw, th),
    6,
  );
  for (let i = 0; i < bg.data.length; i++) bg.data[i]! *= 0.6;

  const scale = Math.min(tw / src.width, th / src.height);
  const fw = Math.max(1, Math.round(src.width * scale));
  const fh = Math.max(1, Math.round(src.height * scale));
  const fg = resampleRect(src, 0, 0, src.width, src.height, fw, fh);
  const out = newRgb(tw, th);
  paste(out, bg, 0, 0);
  paste(out, fg, Math.floor((tw - fw) / 2), Math.floor((th - fh) / 2));
  return out;
}

/** A 64x60 thumbnail filling its cell from the upright sticker image
 *  (the centre crop of PIL's ImageOps.fit). */
export function thumbnail(upright: RgbImage, w: number, h: number): RgbImage {
  const c = coverSize(upright.width, upright.height, w / h);
  return resampleRect(upright, (upright.width - c.w) / 2, (upright.height - c.h) / 2, c.w, c.h, w, h);
}
