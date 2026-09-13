// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Messages between the page and the processing worker.

import type { CropState } from "../core/crop.ts";

export interface BuildItem {
  id: number;
  crop: CropState;
}

export interface IndexedFrame {
  palette: Uint16Array;
  pixels: Uint8Array;
}

export type ToWorker =
  | { type: "load"; id: number; file: Blob; name: string }
  | { type: "remove"; id: number }
  | { type: "render"; id: number; seq: number; crop: CropState }
  | { type: "editor"; id: number }
  | { type: "pages"; seq: number; items: BuildItem[] }
  | { type: "build"; items: BuildItem[]; template: ArrayBuffer; created: string };

export type FromWorker =
  | { type: "loaded"; id: number; width: number; height: number; crop: CropState; thumb: ImageBitmap }
  | { type: "loadError"; id: number; message: string }
  | { type: "rendered"; id: number; seq: number; frame: IndexedFrame; orientation: number }
  | { type: "editor"; id: number; bitmap: ImageBitmap }
  | { type: "pages"; seq: number; pages: IndexedFrame[] }
  | { type: "progress"; done: number; total: number }
  | { type: "built"; rom: ArrayBuffer; photos: number; pages: number }
  | { type: "buildError"; message: string };
