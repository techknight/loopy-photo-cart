# Loopy Photo Cart

Turn your photos into a Casio Loopy cartridge: browse them as pages of
thumbnails or full screen, and print any of them on the Loopy's sticker
printer.

**Make your own at <https://loopyphotocart.com>.** Everything runs in your
browser. Your photos are never uploaded.

## Demo Video

[![Photo of a pastry displayed via Loopy Photo Cart](https://img.youtube.com/vi/2INZK66mI3E/0.jpg)](https://www.youtube.com/watch?v=2INZK66mI3E)

## Making a cartridge

1. Open <https://loopyphotocart.com> and add up to 54 photos.
2. Adjust each crop if you like. The preview shows exactly what the Loopy
   will show on screen and print on a sticker.
3. Pick a format and press **Build ROM**:
   - **Loopy** for the
     [Floopy Drive](https://ko-fi.com/partlyhuman/shop) and emulators like
     LoopyMSE;
   - **MAME** for a byteswapped ROM, for emulators like MAME.

To try it first, the [latest release](https://github.com/techknight/loopy-photo-cart/releases/latest)
has a demo ROM of cat photos: `loopy-photo-cart-demo.bin`, and
`loopy-photo-cart-demo-byteswapped.bin` for MAME. Emulators need the Loopy
BIOS, which isn't included.

## On the Loopy

- **Grid:** pages of 3×3 thumbnails.
  - D-pad moves the cursor.
  - L/R turn pages.
  - A opens a photo.
  - Start prints it.
- **Viewer:** full-screen photos.
  - Left/Right or L/R step through them.
  - A or Start prints.
  - B goes back to the grid.
- **Anywhere:** C turns the music on or off, and D shows help.
- **Stickers:** every photo fills the sticker. Portrait photos print as
  portrait stickers.
- **Music:** Burgmüller's *La Candeur* and Clementi's Sonatina Op. 36 No. 1
  play in turn.

## Building from source

- **Web app:** `cd web && npm install && npm run dev`. See
  [docs/web.md](docs/web.md).
- **Cartridge program:** C for the Loopy's SH-1, built with the Wonderful
  Toolchain. See [cart/README.md](cart/README.md).
- **Demo ROM:** `cd web && npm run demo`.

| Path | What it is |
|---|---|
| `cart/` | The cartridge program, and the Python reference tools |
| `web/` | The web app (TypeScript) |
| `demo/` | The demo ROM's photos |
| `docs/web.md` | How the web app is built, tested and deployed |
| `docs/pack-format.md` | The photo pack format inside a ROM |

## Licence

GPL v2 or later. See [COPYING.md](COPYING.md), and [NOTICE.md](NOTICE.md) for
third-party credits: fonts by GGBotNet, LoopyDOOM, Kasami's homebrew template,
and the music.

ROMs you build contain this GPL program plus your own photos. Your photos
remain yours.
