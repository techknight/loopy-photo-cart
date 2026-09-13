// Copyright (C) 2026 Derek Quenneville
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Offline support. The site is entirely static and processes photos locally,
// so once it has loaded it can run with no network at all. Everything is
// same-origin: the page, its hashed script, style and font assets, the worker
// and the cartridge template.
//
// Strategy: network first, falling back to the cache, so a new deployment is
// picked up whenever the network is there, and every successful same-origin
// GET refreshes the cache. Photos never pass through here; they are read from
// local files and never fetched.

const CACHE = "loopy-photo-cart-v1";

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE).then((cache) =>
      cache.addAll(["./", "./index.html", "./template/loopy-photo-cart-template.bin"]),
    ),
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim()),
  );
});

self.addEventListener("fetch", (event) => {
  const request = event.request;
  if (request.method !== "GET" || new URL(request.url).origin !== self.location.origin) {
    return;
  }
  event.respondWith(
    fetch(request)
      .then((response) => {
        if (response.ok) {
          const copy = response.clone();
          caches.open(CACHE).then((cache) => cache.put(request, copy));
        }
        return response;
      })
      .catch(() =>
        caches.match(request).then((cached) => cached ?? caches.match("./index.html")),
      ),
  );
});
