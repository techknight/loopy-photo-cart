// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later

import { defineConfig } from "vitest/config";

// Built pages allow nothing but their own files: no third-party scripts, no
// network requests except fetching the site's own cartridge template. The
// dev server's hot reload injects inline scripts, so the policy is added to
// production builds only.
const CSP = [
  "default-src 'self'",
  "script-src 'self'",
  "style-src 'self'",
  "img-src 'self' data: blob:",
  "font-src 'self' data:",
  "worker-src 'self' blob:",
  "connect-src 'self'",
  "object-src 'none'",
  "base-uri 'self'",
  "form-action 'none'",
].join("; ");

export default defineConfig({
  // GitHub Pages serves from /<repo>/; a relative base works there and locally.
  base: process.env.BASE_PATH ?? "./",
  build: { target: "es2022" },
  worker: { format: "es" },
  plugins: [
    {
      name: "loopy-csp",
      apply: "build",
      transformIndexHtml(html: string) {
        return html.replace("<!--CSP-->", `<meta http-equiv="Content-Security-Policy" content="${CSP}" />`);
      },
    },
  ],
  test: {
    include: ["test/**/*.test.ts"],
    environment: "node",
  },
});
