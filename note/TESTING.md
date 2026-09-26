# Testing Notes

Run the Python tests with:

```text
python -m pytest -q
```

The test suite covers:

- LZSS compression/decompression round trips, including random data.
- Rejection of truncated LZSS input.
- Basic MML compilation and loop handling.
- SVG rendering through the PC-8801 picture renderer.
- Smooth / Enhanced rendering for both Colour and Monochrome palettes.
- Static checks for the DOS text token-buffer safety changes.
- FAT12 geometry, boot signature, and duplicated FAT tables for every supported floppy size.
- Floppy planning, checksums, and missing-file validation.

Manual checks performed for this release:

- Python byte-compilation of all Python modules.
- `python sgso_convert.py --help`.
- LZSS round-trip tests on empty, repetitive, sequential, and random inputs.
- FAT12 image generation for 360K, 720K, 1.2M, 1.44M, and 2.88M.
- Source and bundled `.pyz` entry point were compared before rebuilding.

## DOS limitation

`SG8.EXE`, `SETUP.EXE`, and `INSTALL.EXE` are 16-bit DOS programs. Open Watcom is not
installed in the current development environment, and it has no internet access to
fetch it, so none of the three could be compiled or run natively here.

### What was actually verified for the 1.4 fixes

Since the real compiler isn't available, `dos_src/sg8.c`, `install.c` and `setup.c`
were syntax-checked with `gcc -fsyntax-only` against a small set of stub headers
(`dos.h`, `conio.h`, `bios.h`, `malloc.h`) that reproduce Open Watcom's documented
`union REGS`/`struct SREGS`/`int86`/`_bios_keybrd`/etc. declarations, with `far`,
`__far`, `__interrupt` and similar keywords defined away. This cannot catch
Watcom-specific semantics (its `#pragma aux` inline-asm blocks, exact memory-model
layout, `far`-pointer segment arithmetic) or, obviously, runtime behaviour -- it only
confirms the code is free of undeclared identifiers, missing prototypes and similar
mechanical errors a real compiler would also reject.

Under that check, `sg8.c` had exactly one hard error before the fix in this release:
`INTR_ZF` was undeclared at the `keyready()` site (see Release Notes 1.4). With
`keyready()` fixed, all three `dos_src/*.c` files pass with zero errors and zero
warnings against the stub headers. This is a meaningful signal (the file could not
have compiled with Open Watcom either, in this exact state) but it is not the same as
a real build, and it says nothing about runtime correctness of the video/keyboard
fixes -- that still needs the steps below.

For a real-hardware release, the final recommended validation is:

1. Run the converter from a clean Python environment.
2. Generate at least one floppy set and one `SG8DOS` folder.
3. Rebuild `dos/SG8.EXE` from `dos_src/sg8.c` with Open Watcom:
   `wcl -bt=dos -ml -0 -ox sg8.c`
4. Boot DOSBox (with a VGA machine type -- `machine=svga_s3` or `machine=vga`, not
   `hercules`/`cga`) or a real DOS PC.
5. Run `INSTALL`, then `SETUP`, then `SG8`.
6. Test save/load, music, sound effects, picture drawing, and each supported floppy
   size that you intend to distribute.
7. Specifically re-test the two reported issues:
   - Typewriter text at a few different `/C:` speeds, mashing keys during typing, to
     confirm characters no longer skip/batch unexpectedly.
   - A monochrome VGA monitor (or DOSBox `machine=vga` with `/GREY`) to confirm the
     picture now displays; if the video BIOS genuinely can't provide mode 12h (a true
     MDA/Hercules/CGA/EGA adapter, or an emulator set to one of those machine types),
     the game should now print a clear message instead of leaving a black screen.
