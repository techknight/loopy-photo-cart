// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// End-to-end smoke test of the built site in headless Chromium: add photos,
// check the previews render, build a ROM and save it, and fail on any page
// error or any request that leaves the site's origin.
//
// Usage: npm run build && node scripts/smoke.mjs out-dir photo1.jpg photo2.jpg ...
// (needs `npx playwright install chromium` once)

import { mkdirSync, readFileSync } from "node:fs";
import { chromium } from "playwright";
import { preview } from "vite";

const [outDir, ...photos] = process.argv.slice(2);
if (!outDir || photos.length === 0) {
  console.error("usage: node scripts/smoke.mjs out-dir photo.jpg ...");
  process.exit(2);
}
mkdirSync(outDir, { recursive: true });

const server = await preview({ preview: { port: 4173, strictPort: true } });
const origin = "http://localhost:4173";
const problems = [];

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ acceptDownloads: true, viewport: { width: 1400, height: 1000 } });
  page.on("pageerror", (e) => problems.push(`page error: ${e.message}`));
  page.on("console", (m) => {
    if (m.type() === "error") problems.push(`console error: ${m.text()}`);
  });
  page.on("request", (r) => {
    const url = r.url();
    if (!url.startsWith(origin) && !url.startsWith("blob:") && !url.startsWith("data:")) {
      problems.push(`request left the site: ${url}`);
    }
  });

  await page.goto(`${origin}/`);
  await page.setInputFiles("#file", photos);
  await page.waitForFunction((n) => document.querySelectorAll(".item.ready").length === n, photos.length, {
    timeout: 60_000,
  });

  // The selected photo's TV preview must show something other than black.
  await page.waitForFunction(
    () => {
      const c = document.querySelector("#preview");
      const d = c.getContext("2d").getImageData(0, 0, 256, 240).data;
      for (let i = 0; i < d.length; i += 4) if (d[i] + d[i + 1] + d[i + 2] > 30) return true;
      return false;
    },
    null,
    { timeout: 30_000 },
  );
  await page.screenshot({ path: `${outDir}/smoke-editor.png` });

  await page.click("#tab-grid");
  await page.waitForFunction(() => document.querySelector("#grid-label").textContent.startsWith("Page"), null, {
    timeout: 30_000,
  });
  await page.screenshot({ path: `${outDir}/smoke-grid.png` });

  const [download] = await Promise.all([page.waitForEvent("download", { timeout: 120_000 }), page.click("#build")]);
  const romPath = `${outDir}/smoke.bin`;
  await download.saveAs(romPath);
  console.log(`status: ${await page.textContent("#status")}`);
  console.log(`saved ${romPath}`);

  // The MAME format is the same ROM with every 16-bit word byteswapped.
  await page.click("#fmt-mame");
  const [mameDownload] = await Promise.all([
    page.waitForEvent("download", { timeout: 120_000 }),
    page.click("#build"),
  ]);
  const mamePath = `${outDir}/smoke-mame.bin`;
  await mameDownload.saveAs(mamePath);
  if (mameDownload.suggestedFilename() !== "loopy-photo-cart-byteswapped.bin") {
    problems.push(`MAME download named ${mameDownload.suggestedFilename()}`);
  }
  const loopy = readFileSync(romPath);
  const mame = readFileSync(mamePath);
  if (mame.length !== loopy.length || !Buffer.from(loopy).swap16().equals(mame)) {
    problems.push("the MAME ROM is not the Loopy ROM byteswapped");
  }
  console.log(`status: ${await page.textContent("#status")}`);
  console.log(`saved ${mamePath}`);
} catch (e) {
  problems.push(String(e));
} finally {
  await browser.close();
  await server.close();
}

if (problems.length) {
  console.error(problems.join("\n"));
  process.exit(1);
}
console.log("smoke test passed");
