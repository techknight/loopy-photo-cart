# Demo ROM

The demo ROM is a cartridge full of cat photos, built from `photos/` using
`manifest.json`.

## Building it

```
cd web
npm run demo
```

This builds `demo/build/loopy-photo-cart-demo.bin` with the web app's own
image and pack code, and beside it `loopy-photo-cart-demo-byteswapped.bin`,
the same ROM byteswapped for emulators like MAME. The manifest fixes the title and date, so the same
commit always gives the same ROM. Tagged releases build and
publish it (`.github/workflows/release.yml`).

## Updating the photos

The committed photos were made from full-size originals, which aren't in the
repository because they're large and carry phone metadata. To prepare your
own photos the same way:

```
python demo/prepare_photos.py originals/ demo/photos
```

The script:

- rotates each photo upright;
- converts its colours to sRGB;
- shrinks it to 1600 px or less on the long side;
- removes all metadata.

It fails if any metadata survives. It requires Pillow.
