# Demo ROM

The demo ROM is a cartridge full of cat photos, built from `photos/` using
`manifest.json`.

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
