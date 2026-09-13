// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Build a ROM from JPEG files with the same core the web app uses, from Node.
// The browser decodes photos itself; here jpeg-js stands in (JPEG only, no
// EXIF rotation, which the demo photos don't need).
//
// Usage: node scripts/demo-rom.ts out.bin photo1.jpg photo2.jpg ...

import { readFileSync, writeFileSync } from "node:fs";
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

const [out, ...paths] = process.argv.slice(2);
if (!out || paths.length === 0) {
  console.error("usage: node scripts/demo-rom.ts out.bin photo.jpg ...");
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
const rom = buildCartridge(template, rendered, { title: "Demo", created: new Date().toISOString().slice(0, 10) });
const report = validateRom(rom);
if (report.errors.length) {
  console.error(report.errors.join("\n"));
  process.exit(1);
}
writeFileSync(out, rom);
console.log(`${report.photos} photos, ${report.pages} pages, ROM ${rom.length} bytes -> ${out}`);
