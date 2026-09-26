# Beginner Notes

This project is a mix of Python and old-school C/DOS code.

I mainly work with **GDScript**, so Python and DOS C are less familiar parts of the project. The code was also developed with **AI assistance**. That is why these notes focus on explaining the connections instead of assuming expert knowledge.

A useful way to read the project is:

1. Start with `sgso_convert.py`.
2. Read `convert.convert_all()` in `lib/convert.py`.
3. Follow one output at a time:
   - scenario → `build_scn()`
   - music → `build_mus()`
   - font → `build_font()`
   - pictures → `build_images()`
4. Read `lib/disks.py` last because it packages the converted files for DOS.
5. Read `dos_src/*.c` when you want to understand how the DOS files consume those generated formats.

When changing code, run the tests before and after the change. Keep the converter's output formats stable unless the matching DOS program is changed too.
