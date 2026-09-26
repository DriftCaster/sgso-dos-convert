# SG Variant Space Octet DOS Convert Tool

by coffee.crisp. (discord)

> **Beginner / AI-assisted project:** This code was developed with AI assistance. I am a beginner in software development and mainly use GDScript, so the project includes extra notes and tests to make the Python/C parts easier to understand and maintain.

> The source code is fully available here under the MIT license. You are free to read it, study it, modify it, improve it, fork it, or use it as a starting point for your own implementation. If you prefer to make an AI-free version, you're welcome to do so as well. Please keep the applicable copyright and license notices when redistributing MIT-licensed code.

I wanted to play STEINS;GATE 8bit (Variant Space Octet) on my IBM P70-386, so I wrote this. It takes **your own copy** of the English PC version and turns it into a real DOS game that runs on an 8086 or better with VGA, split over floppy disks.

It tries to look and feel like the game's PC-8801 mode:

- 8 colour dithered pictures with scanlines, drawn line by line and then filled in, like the original
- or the green screen look of the MZ-2000 mode
- the game's own PC-8801 font, the 3 line text window and the function key bar
- text typed out letter by letter with the little typing beep
- music on the PC speaker, a piezo beeper, or an AdLib / Sound Blaster (3 voices)

No game files are included here. The tool only works from the files you give it.

## Community

If you'd like to follow the development of my other projects, you can join my Discord server:

**Discord:** https://discord.gg/reDa9HQ4gq

The server is primarily for a game I'm currently developing with **GDScript on Godot 4.7**, as well as general development discussion.

## What you need

- Python 3.8 or newer, with numpy (`pip install numpy`). Smooth / Enhanced mode additionally needs CairoSVG and Pillow (`pip install -r requirements-enhanced.txt`).
- your copy of the game (the `data.xp3` file, the game folder, or the zip it came in)
- on the DOS side: 8086+, DOS 3.3+, VGA, about 420 KB of free memory and 3 to 5 MB of disk space

## Quick start

Download `sgso_convert.pyz` from the Releases page and run it:

```
python sgso_convert.pyz
```

Or grab the whole folder and run `sgso_convert.bat` (Windows) or `./sgso_convert.sh` (Linux).

Pick your game, pick your options, hit Convert. You get:

- `FLOPPIES/DISKn/` folders to copy onto real floppies
- `FLOPPIES/DISKn.IMA` disk images for USB floppy drives, Gotek, or emulators
- `SG8DOS/` if you just want to copy it to a hard disk or play it in DOSBox

The full step by step is in [GUIDE.md](GUIDE.md). Developer notes and architecture documentation are in the [`note/`](note/) folder.

## Options

When converting:

- **Picture palette**: Colour (the 8-colour PC-8801 palette) or Monochrome (the MZ-2000 green-screen palette).
- **Picture rendering**: Authentic (the faithful blocky/dithered renderer) or **Smooth / Enhanced** (anti-aliased SVG rendering). Smooth works with either palette and is explicitly an enhancement, **not** how the original game rendered its pictures.
- Smooth / Enhanced mode shows pictures immediately rather than replaying the original drawing animation.
- **Floppy type**: 3.5" 1.44 MB, 720 KB, 2.88 MB, or 5.25" 1.2 MB, 360 KB
- **Compression**: fewer disks, `INSTALL` unpacks everything on the DOS side
- **Drawing data**: keep it to see pictures being drawn, or leave it out to save space

Game settings (you can change these later with `SETUP` on the DOS machine): screen (colour, grey for plasma and LCD screens, green, amber), scanlines, drawing and text speed, typing beep, sound device, volumes. The build-time picture palette and rendering choice are stored in the generated image data.

With compression on, the full game fits on 2 disks of 1.44 MB, 4 of 720 KB or 6 of 360 KB.

## Command line

```
python sgso_convert.py GAME OUTPUT [options]
python sgso_convert.py --help
```

Example: `python sgso_convert.py sg8.zip out --floppy 720K --pictures mono --sound adlib`

## Folders

- `sgso_convert.py`: the window and the command line
- `lib/`: the converter itself (archive reader, script compiler, picture renderer, music, font, floppy images)
- `dos/`: the DOS programs `SG8.EXE`, `SETUP.EXE` and `INSTALL.EXE`
- `dos_src/`: their C source, built with Open Watcom (`wcl -bt=dos -ml -0 -ox sg8.c`)
- `note/`: architecture, testing, beginner notes, and release notes

## Legal stuff

Fan project, not affiliated with or endorsed by MAGES. or 5pb. STEINS;GATE belongs to its owners. Please only use this with a copy of the game you own.

My code is under the MIT license (see `LICENSE`). `lib/zenglyphs.py` is made from the IPA Gothic font and stays under the IPA Font License (see `IPA_Font_License.txt`).
