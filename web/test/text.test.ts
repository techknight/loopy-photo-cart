// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The grid header must render exactly as cart/tools/lpcimage.py draws it
// (fixture from `python cart/tools/mkgolden_web.py`).

import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

import { drawHeader } from "../src/core/pages.ts";
import { UI_BG } from "../src/core/palette.ts";

describe("grid header text", () => {
  it("matches lpcimage pixel for pixel", () => {
    const pixels = new Uint8Array(256 * 240).fill(UI_BG);
    drawHeader(pixels, 0, 6);
    const golden = new Uint8Array(readFileSync(new URL("./fixtures/header.bin", import.meta.url)));
    let diffs = 0;
    for (let i = 0; i < golden.length; i++) if (golden[i] !== pixels[i]) diffs++;
    expect(diffs).toBe(0);
  });
});
