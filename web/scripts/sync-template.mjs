// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copy the cartridge template from the cart build into the site. The copy is
// committed, so the site builds without the SH-1 toolchain.

import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { fileURLToPath } from "node:url";

const src = fileURLToPath(new URL("../../cart/build/loopy-photo-cart-template.bin", import.meta.url));
const dstDir = fileURLToPath(new URL("../public/template/", import.meta.url));

if (!existsSync(src)) {
  console.error(`No template at ${src}. Build the cart first: cart\\scripts\\build.ps1`);
  process.exit(1);
}
mkdirSync(dstDir, { recursive: true });
copyFileSync(src, `${dstDir}loopy-photo-cart-template.bin`);
console.log(`copied ${src} -> ${dstDir}`);
