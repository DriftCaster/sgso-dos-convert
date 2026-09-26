# Testing

## Automated

```
pip install -r requirements-dev.txt
python -m pytest -q
```

Twelve checks covering: LZSS round-trip and rejection of corrupt streams, the MML
compiler, the picture renderer in both colour and two-colour mode, monochrome
shading, FAT12 image geometry for every floppy format, floppy planning and
checksums, the DOS text buffers and keyboard flush, and whether the shipped `.EXE`
files are older than their sources.

## Manual, in DOSBox

Use `machine=vgaonly`, mount the generated `SG8DOS` folder, and check:

1. `SG8` starts, the title screen appears, `start` begins the game.
2. Pictures are drawn and then filled, and look right, not blank.
3. `SG8 /COLOUR`, `/GREY`, `/GREEN`, `/AMBER` each give the expected tint.
4. Text types out letter by letter; pressing keys repeatedly does not skip lines.
5. `SETUP` detects the display and the sound card, and its sound test plays.
6. `save`, `load`, `menu` and `music` all work.

## Manual, from floppies

Convert with compression on, mount the `.IMA` images, copy every disk into
`C:\SG8DOS`, run `INSTALL`, and confirm it reports OK for every file and rebuilds
the game.

## What has been verified for the current release

Built with Open Watcom and run in DOSBox against a real `data.xp3`:

- colour, grey, green and amber, from both the command line and `SG8.CFG`
- automatic screen detection, matching colour output exactly
- the two-colour dithered build on a green screen, with shading intact
- all 111 pictures decoded from `SG8.IMG` and inspected, colour and two-colour
- typing out text while repeatedly pressing Enter, with no skipped lines
- `SETUP` showing correct display, AdLib and free-memory detection
- a full install from three compressed 720 KB floppy images, then playing
