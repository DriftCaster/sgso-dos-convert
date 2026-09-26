# Release notes

## 1.5

Everything in this release was rebuilt with a real Open Watcom toolchain and then
run in DOSBox against an actual `data.xp3`, rather than checked by reading the
source.

- **Rebuilt the DOS programs.** The `SG8.EXE` shipped in earlier releases was older
  than `dos_src/sg8.c` and did not contain fixes that were already written in the
  source, because no DOS compiler was available when those releases were made. This
  is the cause of most of the reported breakage: black screens and skipped text
  came from running a stale binary. `SG8.EXE`, `SETUP.EXE` and `INSTALL.EXE` are
  now built from the current sources, and a test fails if they ever fall behind
  again.
- **Fixed text skipping.** Keys left in the BIOS buffer from an earlier page wait
  were cancelling the typewriter effect on following lines. The buffer is now
  cleared before each line is typed, so holding or tapping Enter no longer skips
  text. Pressing a key while a line is actually typing still reveals it at once, as
  before.
- **Rewrote the smooth picture mode.** The old one called CairoSVG, which rejects
  this game's SVG files outright, so it failed every time it was chosen, and it
  needed extra native dependencies. The new one is pure numpy: it draws each
  picture at triple resolution, averages it back down and dithers the result into
  the palette, then lays the outlines on top at full resolution so they stay
  sharp. It works with both palettes, keeps the drawing animation, adds no
  dependencies, and costs about 25 seconds for a full conversion.
- **Clearer picture options.** *Shaded* uses the 8-colour PC-8801 palette, which
  also displays as distinct shades on grey, green and amber monitors. *Two-colour
  dithered* is the MZ-2000 look. Both were verified on screen.
- Renamed to SG Variant Space Octet DOS Convert Tool.

### Verified for this release

Colour, grey, green and amber screens from both the command line and `SG8.CFG`;
automatic screen detection; the two-colour build with its shading intact; all 111
pictures decoded and inspected in both modes; text typing out correctly while keys
are pressed repeatedly; `SETUP` detecting display, AdLib and free memory; and a
complete install from three compressed 720 KB floppy images followed by playing the
game. Smooth rendering was checked the same way: all 111 pictures inspected in
colour and two-colour form, then run on DOS in colour and on an amber screen.
