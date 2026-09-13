# Loopy Photo Cart

Turn your photos into a Casio Loopy ROM: browse them as pages of thumbnails or
full screen, and print any of them on the Loopy's sticker printer.

Everything runs locally in your browser. Your photos are never uploaded.

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
- **Stickers:** every photo fills the sticker. Portrait photos print as
  portrait stickers.
- **Music and help:** Satie's *Gymnopédie No. 1* plays in the background.
  C pauses it, and D shows help.
- **Capacity:** up to 54 photos in a 4 MB ROM, for the Floopy Drive.

## Repository

| Path | What it is |
|---|---|
| `cart/` | The Loopy program (C, SH-1). See `cart/README.md` for building and testing. |
| `web/` | The web app that builds ROMs in the browser (TypeScript) |
| `demo/` | The demo ROM's photos |
| `docs/PLAN.md` | Architecture, phases and decisions |
| `docs/pack-format.md` | The photo pack format inside a ROM |
| `docs/hardware-test.md` | The checklist for testing on a real Loopy |

## Status

- **Cart:** the viewer, grid and help are confirmed on hardware. Sound and
  printing work in LoopyMSE; their hardware test is in
  `docs/hardware-test.md`.
- **Web app:** in progress.
- **Hosting and releases:** a public site on GitHub Pages and a demo ROM
  release come last.

## Licence

GPL v2 or later. See [COPYING.md](COPYING.md), and [NOTICE.md](NOTICE.md) for
third-party credits: fonts by GGBotNet, LoopyDOOM, Kasami's homebrew template,
and the music.

ROMs you build contain this GPL program plus your own photos. Your photos
remain yours.
