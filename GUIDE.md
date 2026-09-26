# Guide

From your copy of the game to playing on a DOS PC.

## 1. Install Python

**Windows**: get Python from https://www.python.org/downloads/ . During install, tick **"Add Python to PATH"**. Then open a Command Prompt and type:

```
pip install numpy
```

**Linux**: Python is usually already there. You need numpy and tkinter (for the window):

```
sudo apt install python3-numpy python3-tk
```

(or `pip install --user numpy`)

## 2. Find your game files

The tool needs `data.xp3` from STEINS;GATE 8bit (English). You can point it to:

- the `data.xp3` file itself
- the game folder (it finds `data.xp3` inside)
- the zip the game came in, no need to unzip it

## 3. Convert

Double-click `sgso_convert.pyz` (or run `python sgso_convert.pyz`). If you downloaded the folder version, use `sgso_convert.bat` on Windows or `./sgso_convert.sh` on Linux.

1. **Your game**: click File... or Folder... and pick it.
2. **Output folder**: where the result goes.
3. **Build**:
   - Pictures: Colour, or Monochrome for the green screen look
   - Floppy type: whatever your DOS PC has. An IBM P70 or most 386/486 PCs take 3.5" 1.44 MB.
   - Compression: leave it on unless your DOS PC is really slow (an old 8086 can take a few minutes to unpack)
   - Drawing data: Full shows the pictures being drawn, Light saves a disk
4. **Settings**: pick your screen and sound. Some tips:
   - Laptop with a plasma or LCD screen: Screen = Grey
   - Only a small beeper and no real speaker: Sound = Piezo beeper
   - Sound Blaster or AdLib card: Sound = AdLib / Sound Blaster
5. Click **Convert** and wait a few seconds.

## 4. Make the floppies

Look in the output folder, under `FLOPPIES`.

**Real floppies from the DISKn folders**

Format the floppies on the DOS PC first if you can (`FORMAT A:`). Disks formatted on a USB drive sometimes give "sector not found" errors on old drives. Then copy everything inside `DISK1` to the first floppy, `DISK2` to the second, and so on. Copy the files only, not the folder.

**Disk images (DISKn.IMA)**

- Windows: write them with a tool like RawWrite for Windows or ImageUSB
- Linux: `dd if=DISK1.IMA of=/dev/sdX bs=512` (replace sdX with your floppy drive, be careful)
- Gotek drives: copy the `.IMA` files to the USB stick
- Emulators (86Box, PCem, DOSBox): mount them as floppy images

## 5. Install on the DOS PC

```
C:
MD \SG8DOS
COPY A:*.* C:\SG8DOS
```

Repeat the `COPY` line for every disk. Then:

```
CD \SG8DOS
INSTALL
```

`INSTALL` checks every file. If one is bad it tells you which disk to copy again: copy it and run `INSTALL` again. When it says the installation is complete, you can delete `INSTALL.*` if you want.

## 6. Play

```
SETUP
SG8
```

`SETUP` is optional; it lets you change the screen, sound and speed settings and test the sound.

In the game, type commands in English and press Enter: `look`, `look platform`, `talk nae`, `front`, `back`, `help`... Every line of text waits for a key.

| Key | What it does |
|---|---|
| F1 to F5 | left, back, front, right, phone |
| Shift + F1 to F5 | load, save, look, talk, assistant |
| Up arrow | repeat the last command |
| Esc | clear the line |
| F9 | volume |
| F10 | quit |

Other commands: `save`, `load`, `menu` (back to the title screen), and `music` on the title screen for the music gallery.

## DOSBox

Use the `SG8DOS` folder from the output:

```
mount c C:\path\to\output
c:
cd SG8DOS
SG8
```

DOSBox has a Sound Blaster built in, so pick AdLib / Sound Blaster for the music.

## Problems

**"SG8.EXE is missing" when converting**: the `dos` folder next to the tool is gone or empty. Download again, unzip the whole thing, or just use `sgso_convert.pyz`. Some antivirus programs remove old DOS programs, so add an exception if that happens.

**"No module named numpy"**: run `pip install numpy`.

**The window doesn't open on Linux**: install tkinter (`sudo apt install python3-tk`), or use the command line: `python3 sgso_convert.py --help`.

**"not enough memory" on DOS**: free up conventional memory. Add `DOS=HIGH` to CONFIG.SYS, and load fewer drivers.

**No sound**: check the sound device in `SETUP`. On a piezo beeper, pick "Piezo beeper" or raise the speaker pitch. Low notes are very quiet on those.

**Colours look wrong on a plasma or LCD**: set Screen to Grey in `SETUP`.


## Developer / project notes

This is an **AI-assisted beginner project**. I mainly use GDScript, so Python and DOS C are not my primary languages. The `note/` folder explains the architecture and testing process in plain language.

For development, install the test dependency with `pip install -r requirements-dev.txt`, then run `python -m pytest -q`.
