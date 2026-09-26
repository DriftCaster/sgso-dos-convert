# Beginner notes

I mainly write GDScript. Python and DOS-era C are not my usual languages, and this
project was written with AI assistance, so these notes are here to make the code
readable rather than to assume you already know it.

A good reading order:

1. `sgso_convert.py` — start here; it is the whole flow in one file.
2. `convert.convert_all()` in `lib/convert.py`.
3. One output at a time: scenarios in `build_scn()`, music in `build_mus()`, the
   font in `build_font()`, pictures in `build_images()`.
4. `lib/disks.py` last, because it only packages what the others produced.
5. `dos_src/*.c` when you want to see how the DOS side reads those files.

Two rules that matter more than anything else here:

- The Python converter and the DOS programs share file formats. If you change a
  format on one side, change the other side too, and update
  `note/ARCHITECTURE.md`.
- If you edit anything in `dos_src/`, rebuild the `.EXE` files with Open Watcom
  before committing. A test fails if you forget, because shipping a stale binary
  has already caused real bugs in this project.

Run `python -m pytest -q` before and after any change.
