# Guide

From your copy of the game to playing it on a DOS PC.

## 1. Install Python

**Windows**: install Python from https://www.python.org/downloads/ and tick
**"Add Python to PATH"** during setup. Then open a Command Prompt:

```
pip install numpy
```

**Linux**: Python is usually already installed. You need numpy, and tkinter for the
window:

```
sudo apt install python3-numpy python3-tk
```

## 2. Find your game files

The tool needs `data.xp3` from STEINS;GATE Variant Space Octet (English). You can
point it at:

- the `data.xp3` file itself
- the game folder — it will find `data.xp3` inside
- the zip the game came in, without unzipping it

## 3. Convert

Run `sgso_convert.pyz`, or `sgso_convert.bat` / `./sgso_convert.sh` from the full
folder.

1. **Your game**: click File... or Folder... and choose it.
2. **Output folder**: where the result goes.
3. **Build**:
   - **Pictures**: *Shaded* for normal use. It uses the 8-colour PC-8801 palette,
     and on a monochrome, green or amber monitor it still shows distinct shades.
     *Two-colour dithered* is the MZ-2000 look: pure black and white with dither
     patterns for mid-tones.
   - **Rendering**: *Authentic* for the original's hard edges, or *Smooth* for
     anti-aliased pictures with softer shading and sharp outlines. Smooth takes a
     little longer to convert and is an enhancement, not the original look.
   - **Floppy type**: whatever your DOS PC takes. A P70 or most 386/486 machines
     use 3.5" 1.44 MB.
   - **Compression**: leave it on unless your DOS PC is very slow; unpacking on an
     8086 can take a few minutes, once, at install time.
   - **Drawing data**: *Full* replays the original drawing animation, *Light* saves
     a disk and shows pictures at once.
4. **Settings** (all changeable later with `SETUP`):
   - Plasma or LCD screen: Screen = Grey
   - Green or amber monitor: Screen = Green or Amber
   - Only a small beeper: Sound = Piezo beeper
   - AdLib or Sound Blaster: Sound = AdLib / Sound Blaster
   - Leave Screen on Automatic if you are not sure; the game asks the video BIOS.
5. Press **Convert** and wait. It takes a few seconds, longer with compression on.

## 4. Make the floppies

Everything is in the `FLOPPIES` folder.

**Real floppies from the DISKn folders**

Format the floppies on the DOS PC first if you can (`FORMAT A:`). Floppies
formatted on a USB drive sometimes give "sector not found" on old drives. Then copy
the *contents* of `DISK1` to the first floppy, `DISK2` to the second, and so on.

**Disk images (DISKn.IMA)**

- Windows: write them with RawWrite for Windows or ImageUSB
- Linux: `dd if=DISK1.IMA of=/dev/sdX bs=512` — check `sdX` carefully first
- Gotek drives: copy the `.IMA` files to the USB stick
- Emulators (86Box, PCem, DOSBox): mount them as floppy images

## 5. Install on the DOS PC

```
C:
MD \SG8DOS
COPY A:*.* C:\SG8DOS
```

Repeat the `COPY` line for every disk, then:

```
CD \SG8DOS
INSTALL
```

`INSTALL` checks every file and tells you which disk to copy again if one is bad.
When it finishes you can delete `INSTALL.*` if you want the space back.

## 6. Play

```
SETUP
SG8
```

`SETUP` is optional: it changes the screen, sound and speed settings and can test
the sound.

In the game, type what you want to do in English and press Enter: `look`,
`look platform`, `talk nae`, `front`, `back`, `help`. Each line of text waits for a
key press; pressing a key while a line is typing shows the rest of it at once.

| Key | Does |
|---|---|
| F1 to F5 | left, back, front, right, phone |
| Shift + F1 to F5 | load, save, look, talk, assistant |
| Up arrow | repeat the last command |
| Esc | clear the line |
| F9 | volume |
| F10 | quit |

Other commands: `save`, `load`, `menu` to return to the title screen, and `music`
on the title screen for the music gallery.

## DOSBox

Use the `SG8DOS` folder from the output:

```
mount c C:\path\to\output
c:
cd SG8DOS
SG8
```

DOSBox emulates a Sound Blaster, so AdLib / Sound Blaster works there. Its machine
type must be VGA (`machine=vgaonly` or `machine=svga_s3`); the game needs VGA mode
12h and will say so if it does not get it.

## Problems

**"SG8.EXE is missing" when converting** — the `dos` folder next to the tool is
missing or empty. Extract the whole zip, or use `sgso_convert.pyz`, which has
everything inside one file. Some antivirus software deletes old DOS programs; add
an exception if that happens.

**"No module named numpy"** — run `pip install numpy`.

**The window does not open on Linux** — install tkinter
(`sudo apt install python3-tk`) or use the command line.

**Black screen with sound still playing on DOS** — the machine did not switch to
VGA mode 12h. On an emulator, set the machine type to VGA. On real hardware you
need a VGA card; a monochrome VGA *monitor* is fine, but an MDA, CGA, EGA or
Hercules *adapter* cannot show this mode. Current versions print a message instead
of showing a blank screen.

**"not enough memory" on DOS** — free conventional memory: add `DOS=HIGH` to
`CONFIG.SYS` and load fewer drivers.

**No sound** — check the sound device in `SETUP` and use its sound test. On a small
piezo beeper, choose "Piezo beeper", which plays an octave higher; low notes are
nearly inaudible on those.

**Colours look wrong on a plasma or LCD screen** — set Screen to Grey in `SETUP`.

## Developer notes

`note/ARCHITECTURE.md` explains the code layout, `note/TESTING.md` how it is
tested, and `note/BEGINNER_NOTES.md` how to read the project if you are new to it.
For development: `pip install -r requirements-dev.txt`, then `python -m pytest -q`.
