# Architecture

Two halves: a Python converter that runs on a modern PC, and three DOS programs
that run on the old machine.

## Python side

- `sgso_convert.py` — window and command line. Finds `data.xp3`, calls the
  converter, adds the DOS programs and `SG8.CFG`, then hands everything to
  `lib/disks.py`.
- `lib/convert.py` — the pipeline. Reads the XP3 archive, compiles the scenario
  scripts into `SG8.SCN`, converts music into `SG8.MUS`, builds `SG8.FNT`, and
  renders pictures into `SG8.IMG` with drawing-replay data in `SG8.VEC`.
- `lib/pc88draw.py` — turns the game's SVG pictures into 640x200 indexed images the
  way the original engine does: no anti-aliasing, 2x2 dither tiles for fills,
  doubled strokes, white starting canvas. `mono=True` switches to the two-colour
  MZ-2000 patterns. `smooth=True` rasterizes at triple resolution in RGB, averages
  and ordered-dithers the result, then redraws the outlines sharp; it shares the
  same parsed geometry, so the drawing-replay data is identical either way.
- `lib/mml.py` — parses the game's music notation into PC speaker or OPL2 events.
- `lib/lzss.py` — the compression `INSTALL.EXE` unpacks.
- `lib/disks.py` — splits files into floppy-sized pieces, writes `INSTALL.LST` with
  checksums, and builds FAT12 `.IMA` images.
- `lib/zenglyphs.py` — pre-rendered full-width glyphs for the font builder.

## DOS side

- `dos_src/sg8.c` → `dos/SG8.EXE` — the game.
- `dos_src/setup.c` → `dos/SETUP.EXE` — settings and hardware detection.
- `dos_src/install.c` → `dos/INSTALL.EXE` — checks the floppy pieces, joins them,
  and expands the compressed data.

Built with Open Watcom; the exact command lines are in the README and at the top of
each `.c` file.

## Data flow

```
data.xp3
  -> convert.convert_all()
  -> SG8.SCN / SG8.MUS / SG8.FNT / SG8.IMG / SG8.VEC
  -> plus SG8.EXE, SETUP.EXE, SG8.CFG
  -> disks.plan()
  -> DISKn folders and DISKn.IMA images
```

## File formats

Both sides have to agree on these, so changing one means changing the other:

- `SG8.IMG` — `SG8J`, count, then per picture a `(u32 offset, u32 size)` pair;
  each picture is 200 rows of 3 bit planes, PackBits compressed.
- `SG8.VEC` — `SG8B`, count, then one `u32` offset per picture; drawing elements in
  document order.
- `SG8.SCN`, `SG8.MUS`, `SG8.FNT` — see the builder functions in `lib/convert.py`.
- `INSTALL.LST` — one `P` line per floppy piece and one `O` line per output file,
  each with a size and CRC32.

## Boundary

The converter never contains game data. The user supplies `data.xp3`.
