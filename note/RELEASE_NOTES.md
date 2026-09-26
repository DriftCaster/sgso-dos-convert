# Release notes

## 1.4

- Fixed `keyready()` in `dos_src/sg8.c`: it tested `r.x.cflag & INTR_ZF`, but
  `INTR_ZF` was never defined anywhere in the project, so `sg8.c` could not
  compile at all (confirmed with a syntax-only check against Open Watcom's
  documented `dos.h`/`conio.h` API surface -- see `note/TESTING.md`). Even
  taken at face value, the check was wrong: INT 16h/AH=1 reports "no key
  waiting" through the zero flag, not the carry flag that `int86()` puts in
  `cflag`. This is the most likely source of the reported "text sometimes
  skips ahead": `keyready()` could misreport whether a key was actually
  waiting, causing the typewriter effect to bail out and dump several
  characters at once. Replaced with `_bios_keybrd(_KEYBRD_READY)`, the
  standard, portable way to poll the keyboard buffer.
- Added `require_vga()`, called right after `set_mode(0x12)`: if the video
  BIOS did not actually switch to mode 12h (640x480x16, VGA-only), the game
  now prints a clear text-mode message and exits instead of continuing to
  draw into video memory nobody can see. This turns the reported "black
  screen, but audio still works" into an explicit diagnostic. It cannot,
  by itself, make a non-VGA adapter show a VGA-only mode -- see "Known
  cause of the black-screen report" below.

### Known cause of the black-screen report

`SG8.EXE` requires actual VGA graphics hardware (mode 12h). A genuinely
*monochrome VGA monitor* on a real VGA card works fine -- the signal is
still analog RGB, the monitor just renders it in one colour -- and that is
what `/GREY`, `/GREEN` and `/AMBER` are for. What does **not** work is a
monochrome *adapter* (MDA, Hercules, or CGA/EGA without VGA), or an
emulator set to one of those machine types instead of VGA: mode 12h simply
does not exist there, the BIOS mode-set silently does nothing, and the
previous text screen (blank, since the game had already cleared it) is all
that's left on screen -- while the PC-speaker/AdLib sound code, which never
touches the video BIOS, keeps running normally. That combination -- black
screen, working audio -- is the signature of this mismatch. After this
fix the game will say so instead of leaving a blank screen. If it still
reports the mode as accepted and the screen is still black on real VGA
hardware, that would point to a second, different bug we have not seen
yet and would need a repro to chase further.

## 1.3

- Split picture customization into independent **palette** and **rendering** choices.
- Smooth / Enhanced rendering now works with both Colour and Monochrome palettes.
- Kept Authentic rendering as the default.
- Smooth mode remains optional and requires CairoSVG + Pillow.
- Increased DOS text token buffers and bounded `[LastInput]` expansion so long text cannot silently overwrite the token buffer.
- Improved text wrapping behavior at the right edge of the three-line text box.
- Expanded automated tests to 10 checks.

### Build note

The Python converter and documentation are finalized in this release. Open Watcom is
still not installed in the environment used to prepare this archive (no internet
access to fetch it, either), so **`dos/SG8.EXE` in this archive is still the old,
unmodified binary** and does not contain the 1.4 fixes above -- it could not be
recompiled here. `dos_src/sg8.c` has been checked as thoroughly as possible without
the real compiler (see `note/TESTING.md`) and is believed correct, but it has not
been run. Before relying on it:

1. Rebuild with Open Watcom: `wcl -bt=dos -ml -0 -ox sg8.c` (from `dos_src/`), then
   copy the resulting `SG8.EXE` into `dos/`.
2. Validate in DOSBox (with a VGA machine type) and/or real hardware per
   `note/TESTING.md`, specifically re-testing: the typewriter effect for skipped
   characters, and a monochrome VGA monitor/`/GREY` for the black-screen report.
