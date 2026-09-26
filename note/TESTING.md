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
- FAT12 geometry, boot signature, and duplicated FAT tables for every supported floppy size.
- Floppy planning, checksums, and missing-file validation.

Manual checks performed for this release:

- Python byte-compilation of all Python modules.
- `python sgso_convert.py --help`.
- LZSS round-trip tests on empty, repetitive, sequential, and random inputs.
- FAT12 image generation for 360K, 720K, 1.2M, 1.44M, and 2.88M.
- Source and bundled `.pyz` entry point were compared before rebuilding.

## DOS limitation

`SG8.EXE`, `SETUP.EXE`, and `INSTALL.EXE` are 16-bit DOS programs. Open Watcom is not installed in the current development environment, so they could not be recompiled or executed natively here. The DOS C sources were reviewed, and the Python-side output formats were tested against their documented interfaces.

For a real-hardware release, the final recommended validation is:

1. Run the converter from a clean Python environment.
2. Generate at least one floppy set and one `SG8DOS` folder.
3. Boot DOSBox or a real DOS PC.
4. Run `INSTALL`, then `SETUP`, then `SG8`.
5. Test save/load, music, sound effects, picture drawing, and each supported floppy size that you intend to distribute.
