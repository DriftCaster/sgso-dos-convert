# Release notes

## 1.8 - 16-colour smooth rendering

- Smooth colour pictures are now rendered to a real 4-plane VGA image (16 colour indices).
- Smooth mode uses anti-aliased SVG rasterisation and **does not use ordered/Bayer dithering**.
- Smooth monochrome uses 16 luminance levels instead of a two-colour dither pattern.
- Smooth mode uses the standard 16-colour VGA palette and automatically disables the original dark-copy scanline palette, because those extra palette entries are needed for the 16 source colours.
- The DOS renderer source now decodes/blits all four VGA planes.
- The shipped `dos/SG8.EXE` in the source tree is intentionally not replaced here: it must be rebuilt from `dos_src/sg8.c` with Open Watcom before using the new 4-plane `SG8.IMG` format. Using the old executable with a 4-plane image is not supported and can produce a black screen.
