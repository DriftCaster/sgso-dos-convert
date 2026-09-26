# SG Variant Space Octet DOS Convert Tool

by coffee.crisp

A converter that turns **your own copy** of *STEINS;GATE: Variant Space Octet* (the
English PC release) into a version that runs on a real DOS machine: an 8086 or
better with VGA, split across floppy disks. I wrote it to play the game on my
IBM P70-386.

> **About this project:** the code was written with AI assistance. I am a beginner
> at this kind of programming and mainly work in GDScript, so the Python and DOS C
> parts are not my usual territory. The `note/` folder explains how everything fits
> together. The source is MIT licensed: read it, change it, fork it, or rewrite it
> without AI if you prefer. Bug reports and corrections are welcome.

The DOS version reproduces the game's PC-8801 mode:

- 640x200 pictures in 8 dithered colours with scanlines, drawn stroke by stroke
  then filled in, the way the original does it
- a two-colour dithered mode for the MZ-2000 green screen look
- an optional smooth, anti-aliased rendering mode if you prefer softer pictures
- the game's own PC-8801 font, the three line text window and the function key bar
- text typed out letter by letter with the typing beep
- music on the PC speaker, a piezo beeper, or an AdLib / Sound Blaster

No game data is included. The tool only works from files you already own.

## What you need

- Python 3.8 or newer with numpy (`pip install numpy`)
- your copy of the game: the `data.xp3` file, the game folder, or the zip it came in
- on the DOS side: 8086+, DOS 3.3+, VGA, about 420 KB of free conventional memory,
  and 3 to 5 MB of disk space

## Quick start

Download `sgso_convert.pyz` from the Releases page and run it:

```
python sgso_convert.pyz
```

Or download the whole folder and run `sgso_convert.bat` (Windows) or
`./sgso_convert.sh` (Linux).

Pick your game, pick your options, press Convert. You get:

| Output | Use |
|---|---|
| `FLOPPIES/DISKn/` | copy each folder onto a real floppy |
| `FLOPPIES/DISKn.IMA` | disk images for USB floppy drives, Gotek, or emulators |
| `SG8DOS/` | ready to play: copy to a hard disk, or run it in DOSBox |

Step by step instructions are in [GUIDE.md](GUIDE.md).

## Options

Build options:

- **Pictures**: *Shaded* uses the 8-colour PC-8801 palette, which also renders as
  distinct grey, green or amber shades on a monochrome monitor. *Two-colour
  dithered* is the MZ-2000 look: black and white only, with dither patterns
  standing in for mid-tones.
- **Rendering**: *Authentic* reproduces the original's hard-edged drawing.
  *Smooth* draws each picture at triple resolution, averages it down and dithers
  the result, which softens edges and shading while keeping outlines sharp. It is
  an enhancement, not how the original looked, and needs no extra dependencies.
- **Floppy type**: 3.5" 1.44 MB, 720 KB or 2.88 MB; 5.25" 1.2 MB or 360 KB
- **Compression**: fewer disks; `INSTALL` unpacks everything on the DOS machine
- **Drawing data**: keep it to watch pictures being drawn, or leave it out to save
  a disk

Game settings, changeable later with `SETUP` on the DOS machine: screen (colour,
grey, green, amber, or automatic), scanlines, drawing and text speed, typing beep,
sound device, and separate volumes for music, typing and effects.

With compression on, the full game fits on 2 disks of 1.44 MB, 3 of 720 KB, or
6 of 360 KB.

## Command line

```
python sgso_convert.py GAME OUTPUT [options]
python sgso_convert.py --help
```

Example:

```
python sgso_convert.py sg8.zip out --floppy 720K --pictures mono --sound adlib
```

## Folders

- `sgso_convert.py` — the window and the command line
- `lib/` — the converter: archive reader, script compiler, picture renderer, music,
  font, floppy and disk image writer
- `dos/` — the DOS programs `SG8.EXE`, `SETUP.EXE`, `INSTALL.EXE`
- `dos_src/` — their C source, built with Open Watcom
- `note/` — architecture, testing and beginner notes
- `tests/` — `python -m pytest -q`

## Building the DOS programs

The `.EXE` files in `dos/` are already built. To rebuild them you need
[Open Watcom](https://github.com/open-watcom/open-watcom-v2), then from `dos_src/`:

```
wcl -bt=dos -ml -0 -ox sg8.c     -fe=../dos/SG8.EXE
wcl -bt=dos -ms -0 -ox setup.c   -fe=../dos/SETUP.EXE
wcl -bt=dos -ms -0 -ox install.c -fe=../dos/INSTALL.EXE
```

If you change anything in `dos_src/`, rebuild before committing: the test suite
checks that the binaries are not older than their sources.

## Community

If you want to follow my other projects, my Discord server is here:
https://discord.gg/reDa9HQ4gq — it is mostly about a game I am making in GDScript
on Godot 4.7, plus general development talk.

## Legal

Fan project, not affiliated with or endorsed by MAGES. or 5pb. STEINS;GATE belongs
to its owners. Use this only with a copy of the game you own.

My code is MIT licensed (`LICENSE`). `lib/zenglyphs.py` is derived from the IPA
Gothic font and stays under the IPA Font License (`IPA_Font_License.txt`).
