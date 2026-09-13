// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// All photo processing runs here, off the page's thread.
//
// Photos are decoded one at a time with the browser's own decoder (which
// applies EXIF rotation and converts wide-gamut colour to sRGB), shrunk at
// once to a working copy of at most WORK_MAX pixels on the long side, and the
// full-size bitmap is closed. Only working copies are kept, so 54 large phone
// photos stay within a couple of hundred megabytes.

import {
  buildCartridge,
  defaultCrop,
  renderPages,
  renderPhoto,
  stickerAspect,
  suggestFocus,
  validateRom,
  type CropState,
  type PixelSource,
  type RenderedPhoto,
} from "../core/index.ts";
import type { BuildItem, FromWorker, IndexedFrame, ToWorker } from "./protocol.ts";

const WORK_MAX = 1024;
const EDITOR_MAX = 720;
const LIST_THUMB = 96;

interface Entry {
  width: number;
  height: number;
  rgba: Uint8ClampedArray;
  key?: string;
  rendered?: RenderedPhoto;
}

const scope = self as unknown as {
  postMessage(message: FromWorker, transfer: Transferable[]): void;
  onmessage: ((event: MessageEvent<ToWorker>) => void) | null;
};

function post(message: FromWorker, transfer: Transferable[] = []): void {
  scope.postMessage(message, transfer);
}

const photos = new Map<number, Entry>();
let loading: Promise<void> = Promise.resolve();

function isHeic(file: Blob, name: string): boolean {
  return /hei[cf]/i.test(file.type) || /\.hei[cf]$/i.test(name);
}

function source(e: Entry): PixelSource {
  return { width: e.width, height: e.height, channels: 4, data: e.rgba };
}

async function load(id: number, file: Blob, name: string): Promise<void> {
  let bitmap: ImageBitmap;
  try {
    bitmap = await createImageBitmap(file, { imageOrientation: "from-image" });
  } catch {
    post({
      type: "loadError",
      id,
      message: isHeic(file, name)
        ? "This browser can't open HEIC photos. Use Safari, or export the photo as JPEG."
        : "This file couldn't be opened as an image.",
    });
    return;
  }

  try {
    const scale = Math.min(1, WORK_MAX / Math.max(bitmap.width, bitmap.height));
    const w = Math.max(1, Math.round(bitmap.width * scale));
    const h = Math.max(1, Math.round(bitmap.height * scale));
    const canvas = new OffscreenCanvas(w, h);
    const ctx = canvas.getContext("2d", { willReadFrequently: true })!;
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = "high";
    ctx.drawImage(bitmap, 0, 0, w, h);
    bitmap.close();

    const entry: Entry = { width: w, height: h, rgba: ctx.getImageData(0, 0, w, h).data };
    photos.set(id, entry);

    const orientation = h > w ? "portrait" : "landscape";
    const crop = defaultCrop(w, h, suggestFocus(source(entry), stickerAspect(orientation)));

    const ts = LIST_THUMB / Math.max(w, h);
    const thumbCanvas = new OffscreenCanvas(Math.max(1, Math.round(w * ts)), Math.max(1, Math.round(h * ts)));
    const tctx = thumbCanvas.getContext("2d")!;
    tctx.imageSmoothingQuality = "high";
    tctx.drawImage(canvas, 0, 0, thumbCanvas.width, thumbCanvas.height);
    const thumb = thumbCanvas.transferToImageBitmap();
    post({ type: "loaded", id, width: w, height: h, crop, thumb }, [thumb]);
  } catch (e) {
    bitmap.close();
    post({ type: "loadError", id, message: `Couldn't process this photo: ${String(e)}` });
  }
}

// Photos are always dithered: smooth gradients matter more on a photo than the
// fine grain dithering adds. (Grid pages are not; see core/pages.ts.)
const PHOTO_DITHER = true;

function render(id: number, crop: CropState): RenderedPhoto {
  const entry = photos.get(id);
  if (!entry) throw new Error("That photo is no longer loaded.");
  const key = JSON.stringify(crop);
  if (entry.key !== key || !entry.rendered) {
    entry.rendered = renderPhoto(source(entry), crop, PHOTO_DITHER);
    entry.key = key;
  }
  return entry.rendered;
}

function frameCopy(image: { palette: ArrayLike<number>; pixels: Uint8Array }): IndexedFrame {
  return { palette: Uint16Array.from(image.palette), pixels: image.pixels.slice() };
}

// Renders are coalesced: only the newest request per photo is worked on.
const pendingRenders = new Map<number, Extract<ToWorker, { type: "render" }>>();
let flushScheduled = false;

function flushRenders(): void {
  flushScheduled = false;
  for (const [id, msg] of pendingRenders) {
    pendingRenders.delete(id);
    if (!photos.has(id)) continue;
    const r = render(id, msg.crop);
    const frame = frameCopy(r.photo.image);
    post({ type: "rendered", id, seq: msg.seq, frame, orientation: r.photo.orientation }, [
      frame.palette.buffer,
      frame.pixels.buffer,
    ]);
  }
}

function renderAll(items: BuildItem[], progress: boolean): RenderedPhoto[] {
  return items.map((item, i) => {
    const r = render(item.id, item.crop);
    if (progress) post({ type: "progress", done: i + 1, total: items.length });
    return r;
  });
}

scope.onmessage = (event) => {
  const msg = event.data;
  switch (msg.type) {
    case "load":
      loading = loading.then(() => load(msg.id, msg.file, msg.name));
      break;
    case "remove":
      photos.delete(msg.id);
      pendingRenders.delete(msg.id);
      break;
    case "render":
      pendingRenders.set(msg.id, msg);
      if (!flushScheduled) {
        flushScheduled = true;
        setTimeout(flushRenders, 0);
      }
      break;
    case "editor": {
      const entry = photos.get(msg.id);
      if (!entry) break;
      const scale = Math.min(1, EDITOR_MAX / Math.max(entry.width, entry.height));
      const full = new OffscreenCanvas(entry.width, entry.height);
      full.getContext("2d")!.putImageData(new ImageData(entry.rgba.slice(), entry.width, entry.height), 0, 0);
      const small = new OffscreenCanvas(Math.round(entry.width * scale), Math.round(entry.height * scale));
      const sctx = small.getContext("2d")!;
      sctx.imageSmoothingQuality = "high";
      sctx.drawImage(full, 0, 0, small.width, small.height);
      const bitmap = small.transferToImageBitmap();
      post({ type: "editor", id: msg.id, bitmap }, [bitmap]);
      break;
    }
    case "pages": {
      try {
        const pages = renderPages(renderAll(msg.items, false).map((r) => r.thumb)).map((p) => frameCopy(p.image));
        post(
          { type: "pages", seq: msg.seq, pages },
          pages.flatMap((p) => [p.palette.buffer, p.pixels.buffer]),
        );
      } catch {
        post({ type: "pages", seq: msg.seq, pages: [] });
      }
      break;
    }
    case "build": {
      try {
        const rendered = renderAll(msg.items, true);
        const rom = buildCartridge(new Uint8Array(msg.template), rendered, { created: msg.created });
        const report = validateRom(rom);
        if (report.errors.length) throw new Error(`The ROM failed its own check: ${report.errors[0]}`);
        post({ type: "built", rom: rom.buffer as ArrayBuffer, photos: report.photos, pages: report.pages }, [
          rom.buffer as ArrayBuffer,
        ]);
      } catch (e) {
        post({ type: "buildError", message: e instanceof Error ? e.message : String(e) });
      }
      break;
    }
  }
};
