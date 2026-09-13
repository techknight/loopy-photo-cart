// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The crop editor: the photo with the sticker-shaped crop box over it. Drag to
// move the box, scroll or pinch to zoom. The box can never leave the photo.

import { clampCrop, coverSize, cropRect, stickerAspect, type CropState } from "../core/crop.ts";

export class CropEditor {
  private readonly canvas: HTMLCanvasElement;
  private readonly ctx: CanvasRenderingContext2D;
  private readonly onChange: (crop: CropState) => void;
  private bitmap: ImageBitmap | null = null;
  private workW = 1;
  private workH = 1;
  private crop: CropState | null = null;
  private readonly pointers = new Map<number, { x: number; y: number }>();

  constructor(canvas: HTMLCanvasElement, onChange: (crop: CropState) => void) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d")!;
    this.onChange = onChange;

    canvas.addEventListener("pointerdown", (e) => {
      canvas.setPointerCapture(e.pointerId);
      this.pointers.set(e.pointerId, { x: e.offsetX, y: e.offsetY });
    });
    canvas.addEventListener("pointermove", (e) => this.move(e));
    const end = (e: PointerEvent) => this.pointers.delete(e.pointerId);
    canvas.addEventListener("pointerup", end);
    canvas.addEventListener("pointercancel", end);
    canvas.addEventListener(
      "wheel",
      (e) => {
        if (!this.crop || this.crop.mode === "fit") return;
        e.preventDefault();
        this.update({ ...this.crop, zoom: this.crop.zoom * Math.exp(-e.deltaY * 0.0015) });
      },
      { passive: false },
    );
    new ResizeObserver(() => this.draw()).observe(canvas);
  }

  /** Show a photo: `bitmap` is a preview of the working copy (workW x workH). */
  set(bitmap: ImageBitmap | null, workW: number, workH: number, crop: CropState | null): void {
    this.bitmap = bitmap;
    this.workW = workW;
    this.workH = workH;
    this.crop = crop;
    this.draw();
  }

  setCrop(crop: CropState): void {
    this.crop = crop;
    this.draw();
  }

  private scale(): number {
    const pad = 16;
    return Math.min((this.canvas.width - pad * 2) / this.workW, (this.canvas.height - pad * 2) / this.workH);
  }

  private update(next: CropState): void {
    this.crop = clampCrop(this.workW, this.workH, next);
    this.draw();
    this.onChange(this.crop);
  }

  private move(e: PointerEvent): void {
    const prev = this.pointers.get(e.pointerId);
    if (!prev || !this.crop || this.crop.mode === "fit") return;
    const cssToCanvas = this.canvas.width / this.canvas.clientWidth;
    const now = { x: e.offsetX, y: e.offsetY };

    if (this.pointers.size === 1) {
      const s = this.scale();
      const dx = ((now.x - prev.x) * cssToCanvas) / s;
      const dy = ((now.y - prev.y) * cssToCanvas) / s;
      this.pointers.set(e.pointerId, now);
      this.update({ ...this.crop, cx: this.crop.cx + dx, cy: this.crop.cy + dy });
    } else if (this.pointers.size === 2) {
      const [a, b] = [...this.pointers.entries()];
      const other = a![0] === e.pointerId ? b![1] : a![1];
      const before = Math.hypot(prev.x - other.x, prev.y - other.y);
      const after = Math.hypot(now.x - other.x, now.y - other.y);
      this.pointers.set(e.pointerId, now);
      if (before > 4) this.update({ ...this.crop, zoom: (this.crop.zoom * after) / before });
    }
  }

  draw(): void {
    const { canvas, ctx } = this;
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(rect.width * dpr));
    const h = Math.max(1, Math.round(rect.height * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
    ctx.clearRect(0, 0, w, h);
    if (!this.bitmap || !this.crop) return;

    const s = this.scale();
    const ox = (w - this.workW * s) / 2;
    const oy = (h - this.workH * s) / 2;
    ctx.imageSmoothingQuality = "high";
    ctx.drawImage(this.bitmap, ox, oy, this.workW * s, this.workH * s);

    let box;
    if (this.crop.mode === "fill") {
      const r = cropRect(this.workW, this.workH, this.crop);
      box = { x: ox + r.x * s, y: oy + r.y * s, w: r.w * s, h: r.h * s };
      ctx.fillStyle = "rgba(10, 12, 26, 0.62)";
      ctx.beginPath();
      ctx.rect(ox, oy, this.workW * s, this.workH * s);
      ctx.rect(box.x, box.y, box.w, box.h);
      ctx.fill("evenodd");
    } else {
      // Whole photo: the sticker-shaped frame the photo is fitted into.
      const aspect = stickerAspect(this.crop.orientation);
      const iw = this.workW * s;
      const ih = this.workH * s;
      const frame = iw / ih > aspect ? { w: iw, h: iw / aspect } : { w: ih * aspect, h: ih };
      box = { x: ox + (iw - frame.w) / 2, y: oy + (ih - frame.h) / 2, w: frame.w, h: frame.h };
      void coverSize;
    }

    ctx.strokeStyle = "#ff73a5";
    ctx.lineWidth = Math.max(2, 2 * dpr);
    ctx.strokeRect(box.x, box.y, box.w, box.h);
    ctx.strokeStyle = "rgba(255, 255, 255, 0.35)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 1; i < 3; i++) {
      ctx.moveTo(box.x + (box.w * i) / 3, box.y);
      ctx.lineTo(box.x + (box.w * i) / 3, box.y + box.h);
      ctx.moveTo(box.x, box.y + (box.h * i) / 3);
      ctx.lineTo(box.x + box.w, box.y + (box.h * i) / 3);
    }
    ctx.stroke();
  }
}
