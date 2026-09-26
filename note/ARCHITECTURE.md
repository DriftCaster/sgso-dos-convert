# Architecture Notes

This project has two main parts:

1. **Python converter** — reads the user's `data.xp3`, converts scenarios/images/music/fonts, and creates DOS-ready files and floppy images.
2. **DOS runtime** — `SG8.EXE`, `SETUP.EXE`, and `INSTALL.EXE` are the programs that actually run on the target DOS machine.

## Python side

- `sgso_convert.py`
  - GUI and command-line entry point.
  - Finds `data.xp3`.
  - Calls the converter.
  - Adds the DOS executables and creates `SG8.CFG`.
  - Sends the resulting files to `lib/disks.py`.

- `lib/convert.py`
  - Main conversion pipeline.
  - Reads XP3 files.
  - Compiles scenario scripts into `SG8.SCN`.
  - Converts music into `SG8.MUS`.
  - Builds `SG8.FNT`.
  - Renders pictures into `SG8.IMG` and drawing replay data into `SG8.VEC`.

- `lib/pc88draw.py`
  - Converts supported SVG picture primitives into the 640x200 PC-8801-style indexed image.
  - Handles colour dithering and monochrome rendering.

- `lib/mml.py`
  - Parses the game's music notation.
  - Produces PC-speaker or OPL2 events.

- `lib/lzss.py`
  - Compresses files for the DOS installer.
  - Decompresses the same format for verification/tests.

- `lib/disks.py`
  - Splits files into floppy-sized pieces.
  - Writes `INSTALL.LST`.
  - Creates FAT12 `.IMA` disk images.

- `lib/zenglyphs.py`
  - Contains pre-rendered full-width glyphs used by the DOS font builder.

## DOS side

- `dos_src/sg8.c` → `dos/SG8.EXE`
  - Game runtime.
- `dos_src/setup.c` → `dos/SETUP.EXE`
  - Configuration and sound/hardware test utility.
- `dos_src/install.c` → `dos/INSTALL.EXE`
  - Checks floppy pieces, joins them, and expands LZSS data.

The DOS C programs require an old-DOS-compatible compiler such as Open Watcom. The current development environment does not include that compiler, so the C sources were inspected but not natively rebuilt here.

## Data flow

`data.xp3`
→ `convert.convert_all()`
→ `SG8.SCN / SG8.MUS / SG8.FNT / SG8.IMG / SG8.VEC`
→ add DOS executables + `SG8.CFG`
→ `disks.plan()`
→ `DISKn` folders and/or FAT12 `.IMA` images

## Important compatibility boundary

The converter deliberately does **not** include the commercial game data. The user supplies their own copy of `data.xp3`.
