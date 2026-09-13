// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Build a ROM from JPEG files with the same core the web app uses, from Node.
// The browser decodes photos itself; here jpeg-js stands in (JPEG only, no
// EXIF rotation -- the demo photos are stored upright by
// demo/prepare_photos.py).
//
// Usage:
//   node scripts/demo-rom.ts out.bin photo1.jpg photo2.jpg ...
//   node scripts/demo-rom.ts out.bin --manifest ../demo/manifest.json
//
// With a manifest the output is reproducible: its title and date are used
// instead of today's date, and photo paths are relative to the manifest.
// Photos are always dithered, as in the web app.

import { readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import jpeg from "jpeg-js";

import {
  buildCartridge,
  defaultCrop,
  renderPhoto,
  resize,
  stickerAspect,
  suggestFocus,
  validateRom,
  type PixelSource,
} from "../src/core/index.ts";

const WORK_MAX = 1024;

interface Manifest {
  title?: string;
  created?: string;
  photos: string[];
}

const args = process.argv.slice(2);
const out = args.shift();
let paths: string[] = [];
let title = "Demo";
let created: string = new Date().toISOString().slice(0, 10);

if (args[0] === "--manifest" && args[1]) {
  const manifestPath = resolve(args[1]);
  const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as Manifest;
  paths = manifest.photos.map((p) => resolve(dirname(manifestPath), p));
  title = manifest.title ?? title;
  created = manifest.created ?? created;
} else {
  paths = args;
}

if (!out || paths.length === 0) {
  console.error("usage: node scripts/demo-rom.ts out.bin photo.jpg ...");
  console.error("       node scripts/demo-rom.ts out.bin --manifest manifest.json");
  process.exit(2);
}

const rendered = paths.map((path) => {
  const raw = jpeg.decode(readFileSync(path), { useTArray: true, maxMemoryUsageInMB: 2048 });
  const full: PixelSource = { width: raw.width, height: raw.height, channels: 4, data: raw.data };
  const scale = Math.min(1, WORK_MAX / Math.max(raw.width, raw.height));
  const work = resize(full, Math.round(raw.width * scale), Math.round(raw.height * scale));
  const orientation = work.height > work.width ? "portrait" : "landscape";
  const crop = defaultCrop(work.width, work.height, suggestFocus(work, stickerAspect(orientation)));
  console.log(`${path}: ${raw.width}x${raw.height}, ${crop.orientation}`);
  return renderPhoto(work, crop, true);
});

const template = new Uint8Array(readFileSync(new URL("../public/template/loopy-photo-cart-template.bin", import.meta.url)));
const rom = buildCartridge(template, rendered, { title, created });
const report = validateRom(rom);
if (report.errors.length) {
  console.error(report.errors.join("\n"));
  process.exit(1);
}
writeFileSync(out, rom);
console.log(`${report.photos} photos, ${report.pages} pages, ROM ${rom.length} bytes -> ${out}`);
