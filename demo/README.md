# Demo ROM

The demo ROM is a cartridge full of cat photos, built from `photos/` using
`manifest.json`.

## Building it

```
cd web
npm run demo
```

This builds `demo/build/loopy-photo-cart-demo.bin` with the web app's own
image and pack code. The manifest fixes the title, date and dither setting,
so the same commit always gives the same ROM. Tagged releases build and
publish it (`.github/workflows/release.yml`).

## Updating the photos

The full-size originals live in `../sample-photos/`. That folder is
git-ignored because the originals are large and carry phone metadata.
Regenerate the committed copies with:

```
python demo/prepare_photos.py sample-photos demo/photos
```

The script:

- rotates each photo upright;
- converts its colours to sRGB;
- shrinks it to 1600 px or less on the long side;
- removes all metadata.

It fails if any metadata survives. It requires Pillow.
