// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later

import {
  DESCRIPTOR_MAGIC,
  DESCRIPTOR_OFFSET,
  DESCRIPTOR_SIZE,
  DESCRIPTOR_VERSION,
  ROM_BASE,
} from "./constants.ts";

/** The template's 32-byte descriptor at ROM offset 0x20 (cart/README.md). */
export interface TemplateDescriptor {
  descriptorVersion: number;
  /** The pack format version the cartridge program reads. */
  packVersion: number;
  /** Address the pack must start at. */
  packBase: number;
  /** End of the cartridge address window (4 MB for the Floopy Drive). */
  romLimit: number;
  buildId: string;
}

export class TemplateError extends Error {
  override name = "TemplateError";
}

function ascii(bytes: Uint8Array): string {
  return String.fromCharCode(...bytes);
}

export function readDescriptor(template: Uint8Array): TemplateDescriptor {
  if (template.byteLength < DESCRIPTOR_OFFSET + DESCRIPTOR_SIZE) {
    throw new TemplateError("The template is too short to be a Loopy Photo Cart ROM.");
  }
  const view = new DataView(template.buffer, template.byteOffset, template.byteLength);

  const magic = ascii(template.subarray(DESCRIPTOR_OFFSET, DESCRIPTOR_OFFSET + 4));
  if (magic !== DESCRIPTOR_MAGIC) {
    throw new TemplateError("This is not a Loopy Photo Cart template (no LPCT descriptor).");
  }

  const descriptorVersion = view.getUint16(DESCRIPTOR_OFFSET + 4);
  if (descriptorVersion !== DESCRIPTOR_VERSION) {
    throw new TemplateError(`Unsupported template descriptor version ${descriptorVersion}.`);
  }

  const packVersion = view.getUint16(DESCRIPTOR_OFFSET + 6);
  const packBase = view.getUint32(DESCRIPTOR_OFFSET + 8);
  const romLimit = view.getUint32(DESCRIPTOR_OFFSET + 12);
  if (packBase <= ROM_BASE || packBase % 4 !== 0 || romLimit <= packBase) {
    throw new TemplateError("The template descriptor's pack region is invalid.");
  }

  const idBytes = template.subarray(DESCRIPTOR_OFFSET + 16, DESCRIPTOR_OFFSET + 32);
  const nul = idBytes.indexOf(0);
  const buildId = ascii(nul < 0 ? idBytes : idBytes.subarray(0, nul));

  return { descriptorVersion, packVersion, packBase, romLimit, buildId };
}
