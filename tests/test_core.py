import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "lib"))

import disks
import lzss
import mml
import pc88draw


def test_lzss_roundtrip():
    rng = random.Random(12345)
    samples = [b"", b"a", b"abc", b"a" * 10000, bytes(range(256)) * 40]
    samples += [bytes(rng.randrange(256) for _ in range(n))
                for n in (1, 2, 3, 10, 100, 1000, 10000)]
    for data in samples:
        assert lzss.decompress(lzss.compress(data), len(data)) == data


def test_lzss_rejects_truncated_data():
    try:
        lzss.decompress(b"\x00", 1)
    except ValueError as exc:
        assert "truncated" in str(exc)
    else:
        raise AssertionError("truncated stream was accepted")


def test_mml_compiler():
    events, loop, duration = mml.compile_mml("t120 l4 o4 L c d e f")
    assert events
    assert loop == 0
    assert duration > 0


def test_pc88draw_simple_svg():
    svg = b'''<svg xmlns="http://www.w3.org/2000/svg" width="640" height="400">
      <rect x="0" y="0" width="640" height="200" fill="#ff0000"/>
      <line x1="0" y1="0" x2="639" y2="199" stroke="#ffffff"/>
    </svg>'''
    idx, elements = pc88draw.render(svg)
    assert idx.shape == (200, 640)
    assert idx.min() >= 0 and idx.max() <= 7
    assert elements


def test_fat12_images_have_valid_geometry():
    files = {"SG8.EXE": b"abc" * 100, "INSTALL.LST": b"x" * 700}
    for fmt in disks.FORMATS:
        image = disks.fat12_image(files, "TEST", fmt)
        g = disks.geometry(fmt)
        assert len(image) == g["total"] * 512
        assert image[510:512] == b"\x55\xAA"
        fat1 = image[512:512 + g["spf"] * 512]
        fat2 = image[(1 + g["spf"]) * 512:(1 + 2 * g["spf"]) * 512]
        assert fat1 == fat2


def test_floppy_plan_records_piece_and_output_checksums():
    files = {"INSTALL.EXE": b"installer", "INSTALL.TXT": b"read me", "SG8.EXE": b"A" * 5000}
    disks_out = disks.plan(files, ["SG8.EXE"], "1.44M", compress=True, log=lambda *_: None)
    assert disks_out
    assert "INSTALL.LST" in disks_out[0]
    text = disks_out[0]["INSTALL.LST"].decode("ascii")
    assert "P SG8.EX" in text
    assert "O SG8.EXE 5000" in text


def test_floppy_plan_rejects_missing_ordered_file():
    try:
        disks.plan({}, ["SG8.EXE"], "1.44M", log=lambda *_: None)
    except ValueError as exc:
        assert "missing file" in str(exc)
    else:
        raise AssertionError("missing file was accepted")


def test_pc88draw_smooth_optional():
    # Smooth mode is optional and must not become a requirement for normal use.
    try:
        import cairosvg  # noqa: F401
        import PIL  # noqa: F401
    except ImportError:
        return
    svg = b'''<svg xmlns="http://www.w3.org/2000/svg" width="640" height="400">
      <rect x="0" y="0" width="640" height="400" fill="#ffffff"/>
      <path d="M 40 40 L 600 80 L 300 360 Z" fill="#ff0000"/>
    </svg>'''
    idx, elements = pc88draw.render(svg, smooth=True)
    assert idx.shape == (200, 640)
    assert idx.min() >= 0 and idx.max() <= 7
    assert elements == []


def test_pc88draw_smooth_monochrome_optional():
    try:
        import cairosvg  # noqa: F401
        import PIL  # noqa: F401
    except ImportError:
        return
    svg = b'''<svg xmlns="http://www.w3.org/2000/svg" width="640" height="400">
      <rect x="0" y="0" width="640" height="400" fill="#808080"/>
    </svg>'''
    idx, elements = pc88draw.render(svg, mono=True, smooth=True)
    assert idx.shape == (200, 640)
    assert set(idx.flat).issubset({0, 7})
    assert elements == []


def test_text_runtime_has_no_silent_token_overflow():
    source = (ROOT / "dos_src" / "sg8.c").read_text(encoding="utf-8")
    assert "static Tok t[1024]" in source
    assert "m < 1024 - 1" in source
    assert "m < 1024 - MAX_INPUT" in source
