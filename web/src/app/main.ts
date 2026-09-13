// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// The Loopy Photo Cart web app. Everything happens in this tab: photos are
// read from disk, processed in a worker and turned into a ROM in memory.
// Nothing is uploaded anywhere.

import "./style.css";

import {
  clampCrop,
  defaultCrop,
  keptFraction,
  LOW_KEPT_FRACTION,
  MAX_PHOTOS,
  storedRgba,
  tvRgba,
  type CropState,
  type Orientation,
} from "../core/index.ts";
import { CropEditor } from "./cropEditor.ts";
import type { BuildItem, FromWorker, IndexedFrame, ToWorker } from "./protocol.ts";

interface Item {
  id: number;
  name: string;
  status: "loading" | "ready" | "error";
  message?: string;
  width: number;
  height: number;
  crop?: CropState;
  autoCrop?: CropState;
  dither: boolean;
  thumb?: ImageBitmap;
}

type Tab = "tv" | "sticker" | "grid";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const worker = new Worker(new URL("./worker.ts", import.meta.url), { type: "module" });
const send = (msg: ToWorker, transfer: Transferable[] = []) => worker.postMessage(msg, transfer);

let items: Item[] = [];
let selected: number | null = null;
let nextId = 1;
let tab: Tab = "tv";
let renderSeq = 0;
let pagesSeq = 0;
let lastFrame: { id: number; frame: IndexedFrame; orientation: number } | null = null;
let pages: IndexedFrame[] = [];
let pageIndex = 0;
let building = false;
let editorBitmap: ImageBitmap | null = null;

const editor = new CropEditor($("editor"), (crop) => {
  const item = current();
  if (!item) return;
  item.crop = crop;
  scheduleRender();
  refreshList();
  refreshTools();
});

function current(): Item | undefined {
  return items.find((i) => i.id === selected);
}

function readyItems(): Item[] {
  return items.filter((i) => i.status === "ready");
}

function buildItems(): BuildItem[] {
  return readyItems().map((i) => ({ id: i.id, crop: i.crop!, dither: i.dither }));
}

function setStatus(text: string, kind: "" | "error" | "ok" = ""): void {
  const el = $("status");
  el.textContent = text;
  el.className = `status ${kind}`;
}

// ---------------------------------------------------------------------------
// Adding photos

function addFiles(files: Iterable<File>): void {
  const list = [...files];
  const room = MAX_PHOTOS - items.length;
  const accepted = list.slice(0, Math.max(0, room));
  for (const file of accepted) {
    const item: Item = { id: nextId++, name: file.name, status: "loading", width: 0, height: 0, dither: true };
    items.push(item);
    send({ type: "load", id: item.id, file, name: file.name });
  }
  if (list.length > accepted.length) {
    setStatus(`A cartridge holds ${MAX_PHOTOS} photos, so ${list.length - accepted.length} weren't added.`, "error");
  } else if (accepted.length) {
    setStatus("");
  }
  if (selected === null && accepted.length) selected = items[items.length - accepted.length]!.id;
  refreshAll();
}

const fileInput = $<HTMLInputElement>("file");
fileInput.addEventListener("change", () => {
  if (fileInput.files) addFiles(fileInput.files);
  fileInput.value = "";
});

const drop = $("drop");
for (const target of [drop, document.body]) {
  target.addEventListener("dragover", (e) => {
    if (e.dataTransfer?.types.includes("Files")) {
      e.preventDefault();
      drop.classList.add("over");
    }
  });
}
document.body.addEventListener("dragleave", (e) => {
  if (!e.relatedTarget) drop.classList.remove("over");
});
document.body.addEventListener("drop", (e) => {
  if (!e.dataTransfer?.files.length) return;
  e.preventDefault();
  drop.classList.remove("over");
  addFiles(e.dataTransfer.files);
});

// ---------------------------------------------------------------------------
// Worker replies

worker.onmessage = (event: MessageEvent<FromWorker>) => {
  const msg = event.data;
  switch (msg.type) {
    case "loaded": {
      const item = items.find((i) => i.id === msg.id);
      if (!item) {
        msg.thumb.close();
        break;
      }
      Object.assign(item, { status: "ready", width: msg.width, height: msg.height, crop: msg.crop, autoCrop: msg.crop, thumb: msg.thumb });
      if (item.id === selected) selectItem(item.id);
      refreshAll();
      invalidatePages();
      break;
    }
    case "loadError": {
      const item = items.find((i) => i.id === msg.id);
      if (item) Object.assign(item, { status: "error", message: msg.message });
      refreshAll();
      break;
    }
    case "rendered":
      if (msg.id === selected && msg.seq === renderSeq) {
        lastFrame = { id: msg.id, frame: msg.frame, orientation: msg.orientation };
        drawPreview();
      }
      break;
    case "editor":
      if (msg.id === selected) {
        editorBitmap?.close();
        editorBitmap = msg.bitmap;
        const item = current();
        if (item) editor.set(editorBitmap, item.width, item.height, item.crop ?? null);
      } else {
        msg.bitmap.close();
      }
      break;
    case "pages":
      if (msg.seq === pagesSeq) {
        pages = msg.pages;
        pageIndex = Math.min(pageIndex, Math.max(0, pages.length - 1));
        drawPreview();
      }
      break;
    case "progress":
      setStatus(`Preparing photo ${msg.done} of ${msg.total}…`);
      break;
    case "built": {
      building = false;
      const blob = new Blob([msg.rom], { type: "application/octet-stream" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "loopy-photo-cart.bin";
      a.click();
      setTimeout(() => URL.revokeObjectURL(a.href), 10_000);
      const mb = (msg.rom.byteLength / 1048576).toFixed(2);
      const plural = (n: number, word: string) => `${n} ${word}${n === 1 ? "" : "s"}`;
      setStatus(
        `Built loopy-photo-cart.bin: ${plural(msg.photos, "photo")} on ${plural(msg.pages, "page")}, ${mb} MB of 4 MB.`,
        "ok",
      );
      refreshBar();
      break;
    }
    case "buildError":
      building = false;
      setStatus(msg.message, "error");
      refreshBar();
      break;
  }
};

// ---------------------------------------------------------------------------
// Selection, crop tools and previews

let renderTimer = 0;
function scheduleRender(): void {
  clearTimeout(renderTimer);
  renderTimer = window.setTimeout(() => {
    const item = current();
    if (!item || item.status !== "ready") return;
    send({ type: "render", id: item.id, seq: ++renderSeq, crop: item.crop!, dither: item.dither });
    invalidatePages();
  }, 120);
}

let pagesTimer = 0;
function invalidatePages(): void {
  if (tab !== "grid") return;
  clearTimeout(pagesTimer);
  pagesTimer = window.setTimeout(() => {
    if (!readyItems().length) {
      pages = [];
      drawPreview();
      return;
    }
    send({ type: "pages", seq: ++pagesSeq, items: buildItems() });
  }, 250);
}

function selectItem(id: number | null): void {
  selected = id;
  lastFrame = null;
  const item = current();
  editor.set(null, 1, 1, null);
  if (item?.status === "ready") {
    send({ type: "editor", id: item.id });
    scheduleRender();
  }
  refreshAll();
}

function changeCrop(update: (c: CropState, item: Item) => CropState): void {
  const item = current();
  if (!item?.crop) return;
  item.crop = clampCrop(item.width, item.height, update(item.crop, item));
  editor.setCrop(item.crop);
  scheduleRender();
  refreshAll();
}

function setOrientation(o: Orientation): void {
  changeCrop((c) => ({ ...c, orientation: o, zoom: 1 }));
}

$("landscape").addEventListener("click", () => setOrientation("landscape"));
$("portrait").addEventListener("click", () => setOrientation("portrait"));
$("fill").addEventListener("click", () => changeCrop((c) => ({ ...c, mode: "fill" })));
$("fit").addEventListener("click", () => changeCrop((c) => ({ ...c, mode: "fit" })));
$("reset").addEventListener("click", () =>
  changeCrop((_c, item) => item.autoCrop ?? defaultCrop(item.width, item.height)),
);
$<HTMLInputElement>("dither").addEventListener("change", (e) => {
  const item = current();
  if (!item) return;
  item.dither = (e.target as HTMLInputElement).checked;
  scheduleRender();
});

for (const t of ["tv", "sticker", "grid"] as const) {
  $(`tab-${t}`).addEventListener("click", () => {
    tab = t;
    if (t === "grid") invalidatePages();
    refreshTabs();
    drawPreview();
  });
}
$("grid-prev").addEventListener("click", () => {
  if (pages.length) pageIndex = (pageIndex + pages.length - 1) % pages.length;
  drawPreview();
});
$("grid-next").addEventListener("click", () => {
  if (pages.length) pageIndex = (pageIndex + 1) % pages.length;
  drawPreview();
});

function drawPreview(): void {
  const canvas = $<HTMLCanvasElement>("preview");
  const ctx = canvas.getContext("2d")!;
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, 256, 240);
  let rgba: Uint8ClampedArray | null = null;
  if (tab === "grid") {
    const page = pages[pageIndex];
    if (page) rgba = storedRgba(page);
    $("grid-label").textContent = pages.length ? `Page ${pageIndex + 1} / ${pages.length}` : "No pages yet";
  } else if (lastFrame && lastFrame.id === selected) {
    rgba = tab === "tv" ? tvRgba(lastFrame.frame, lastFrame.orientation) : storedRgba(lastFrame.frame);
  }
  if (rgba) ctx.putImageData(new ImageData(new Uint8ClampedArray(rgba), 256, 240), 0, 0);
}

// ---------------------------------------------------------------------------
// Rendering the page

function refreshList(): void {
  const list = $("list");
  list.replaceChildren();
  items.forEach((item, index) => {
    const li = document.createElement("li");
    li.className = `item ${item.id === selected ? "selected" : ""} ${item.status}`;
    li.draggable = true;
    li.addEventListener("click", () => selectItem(item.id));
    li.addEventListener("dragstart", (e) => {
      e.dataTransfer?.setData("text/x-loopy-index", String(index));
    });
    li.addEventListener("dragover", (e) => {
      if (e.dataTransfer?.types.includes("text/x-loopy-index")) e.preventDefault();
    });
    li.addEventListener("drop", (e) => {
      const from = Number(e.dataTransfer?.getData("text/x-loopy-index"));
      if (Number.isNaN(from) || !e.dataTransfer?.types.includes("text/x-loopy-index")) return;
      e.preventDefault();
      e.stopPropagation();
      moveItem(from, index);
    });

    const thumb = document.createElement("canvas");
    thumb.className = "thumb";
    thumb.width = 48;
    thumb.height = 48;
    if (item.thumb) {
      const t = thumb.getContext("2d")!;
      const s = Math.max(48 / item.thumb.width, 48 / item.thumb.height);
      t.drawImage(item.thumb, (48 - item.thumb.width * s) / 2, (48 - item.thumb.height * s) / 2, item.thumb.width * s, item.thumb.height * s);
    }

    const info = document.createElement("div");
    info.className = "info";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = `${index + 1}. ${item.name}`;
    const note = document.createElement("span");
    note.className = "note";
    if (item.status === "loading") note.textContent = "Opening…";
    else if (item.status === "error") note.textContent = item.message ?? "Couldn't open";
    else if (item.crop) {
      const kept = keptFraction(item.width, item.height, item.crop);
      const shape = item.crop.orientation === "portrait" ? "Portrait" : "Landscape";
      note.textContent = item.crop.mode === "fit" ? `${shape}, whole photo` : `${shape}, keeps ${Math.round(kept * 100)}%`;
      if (kept < LOW_KEPT_FRACTION) note.classList.add("warn");
    }
    info.append(name, note);

    const actions = document.createElement("div");
    actions.className = "actions";
    const button = (label: string, title: string, fn: () => void, disabled = false) => {
      const b = document.createElement("button");
      b.textContent = label;
      b.title = title;
      b.setAttribute("aria-label", title);
      b.disabled = disabled;
      b.addEventListener("click", (e) => {
        e.stopPropagation();
        fn();
      });
      actions.append(b);
    };
    button("↑", "Move up", () => moveItem(index, index - 1), index === 0);
    button("↓", "Move down", () => moveItem(index, index + 1), index === items.length - 1);
    button("✕", "Remove", () => removeItem(item.id));

    li.append(thumb, info, actions);
    list.append(li);
  });
  $("count").textContent = `${items.length} / ${MAX_PHOTOS}`;
}

function moveItem(from: number, to: number): void {
  if (to < 0 || to >= items.length || from === to) return;
  const [item] = items.splice(from, 1);
  items.splice(to, 0, item!);
  invalidatePages();
  refreshAll();
}

function removeItem(id: number): void {
  const index = items.findIndex((i) => i.id === id);
  if (index < 0) return;
  items[index]!.thumb?.close();
  items.splice(index, 1);
  send({ type: "remove", id });
  if (selected === id) selectItem(items[Math.min(index, items.length - 1)]?.id ?? null);
  invalidatePages();
  refreshAll();
}

function refreshTools(): void {
  const item = current();
  const ready = item?.status === "ready" && item.crop;
  for (const id of ["landscape", "portrait", "fill", "fit", "reset", "dither"]) {
    ($(id) as HTMLButtonElement).disabled = !ready;
  }
  $("editor-empty").hidden = Boolean(ready);
  if (!ready) {
    $("kept").textContent = "";
    return;
  }
  const c = item.crop!;
  $("landscape").classList.toggle("on", c.orientation === "landscape");
  $("portrait").classList.toggle("on", c.orientation === "portrait");
  $("fill").classList.toggle("on", c.mode === "fill");
  $("fit").classList.toggle("on", c.mode === "fit");
  $<HTMLInputElement>("dither").checked = item.dither;
  const kept = keptFraction(item.width, item.height, c);
  $("kept").textContent = c.mode === "fit" ? "Whole photo" : `Keeps ${Math.round(kept * 100)}% of the photo`;
  $("kept").classList.toggle("warn", kept < LOW_KEPT_FRACTION);
}

function refreshTabs(): void {
  for (const t of ["tv", "sticker", "grid"] as const) $(`tab-${t}`).classList.toggle("on", tab === t);
  $("grid-nav").hidden = tab !== "grid";
}

function refreshBar(): void {
  const ready = readyItems().length;
  const loading = items.some((i) => i.status === "loading");
  const b = $<HTMLButtonElement>("build");
  b.disabled = building || loading || ready === 0;
  b.textContent = building ? "Building…" : "Build ROM";
}

function refreshAll(): void {
  refreshList();
  refreshTools();
  refreshTabs();
  refreshBar();
  drawPreview();
}

$("build").addEventListener("click", async () => {
  if (building) return;
  building = true;
  refreshBar();
  setStatus("Loading the cartridge template…");
  try {
    const res = await fetch(new URL("template/loopy-photo-cart-template.bin", document.baseURI));
    if (!res.ok) throw new Error(`The cartridge template couldn't be loaded (${res.status}).`);
    const template = await res.arrayBuffer();
    const skipped = items.length - readyItems().length;
    if (skipped) setStatus(`Leaving out ${skipped} photo${skipped > 1 ? "s" : ""} that couldn't be opened.`);
    send({ type: "build", items: buildItems(), template, created: new Date().toISOString().slice(0, 10) }, [template]);
  } catch (e) {
    building = false;
    setStatus(e instanceof Error ? e.message : String(e), "error");
    refreshBar();
  }
});

refreshAll();
