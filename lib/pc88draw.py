"""Renders the STEINS;GATE 8bit SVGs the way the original engine does in its
recommended PC-8801 mkIISR mode (sg8bit_paint.tjs / sg8bit.tjs):

- the SVG is read as a flat list of elements in document order, with no transforms,
  no styles and no display/visibility handling (SimpleSVGParser);
- drawing is done at 640x200 (y scaled by 0.5), without antialiasing;
- fills use 2x2 dither tiles built from the 8 digital colours (calcDitherColor);
- strokes use the nearest digital colour, drawn twice (x and x+1: DoubleLine);
- the canvas starts white (the last palette colour).
Pure Python + numpy, no graphics library needed.

Returns the final 640x200 image as colour indices 0-7 and the element list used to
replay the drawing (lines, then fills revealed from the final image).
"""
import re
import xml.etree.ElementTree as ET
import numpy as np

W, H = 640, 200
SCALE_X, SCALE_Y = 1.0, 0.5

# Digital 8-colour palette, in the original index order (bit0 blue, bit1 red, bit2 green)
PALETTE = [0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF]
_PAL_RGB = [((c >> 16) & 255, (c >> 8) & 255, c & 255) for c in PALETTE]
_PAL_POS = [(0.29891 * r / 255, 0.58661 * g / 255, 0.11448 * b / 255) for r, g, b in _PAL_RGB]
DITH_COEF = [0.0, 0.25, 0.5, 0.0]


def _col2rgb(col):
    return ((col >> 16) & 255) / 255.0, ((col >> 8) & 255) / 255.0, (col & 255) / 255.0


def rgb2digit(r, g, b):
    r, g, b = (min(max(v, 0.0), 1.0) for v in (r, g, b))
    # rgb2col truncates to 8 bits before measuring, as in the original
    r, g, b = int(r * 255) / 255.0, int(g * 255) / 255.0, int(b * 255) / 255.0
    p = (0.29891 * r, 0.58661 * g, 0.11448 * b)
    best, idx = None, 0
    for i, q in enumerate(_PAL_POS):
        d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
        if best is None or d < best:
            best, idx = d, i
    return idx


def dither_tile(col):
    """calcDitherColor: four colour indices for the 2x2 tile, (x + y*2) order."""
    r, g, b = _col2rgb(col)
    dr = dg = db = 0.0
    cols = []
    for i in range(4):
        f = DITH_COEF[i]
        c = rgb2digit(r + dr * f, g + dg * f, b + db * f)
        cols.append(c)
        if i == 0:
            pr, pg, pb = (v / 255.0 for v in _PAL_RGB[c])
            dr, dg, db = r - pr, g - pg, b - pb
    return cols


def line_colour(col):
    return rgb2digit(*_col2rgb(col))


# Monochrome (green-screen MZ-2000 mode, "Monochrome" method): two colours, 0 = black and
# 7 = the monitor colour; fills use four 2x2 grey patterns.
MONO_PATTERNS = [(0, 0, 0, 0), (0, 0, 7, 0), (0, 7, 7, 0), (0, 7, 7, 7), (7, 7, 7, 7)]


def _grey(col, gamma=1.0):
    r, g, b = _col2rgb(col)
    v = min(max(0.29891 * r + 0.58661 * g + 0.11448 * b, 0.0), 1.0)
    if gamma != 1.0:
        v = v ** (1.0 / gamma)
    return int(v * 255) / 255.0


def mono_tile(col, gamma):
    v = _grey(col, gamma)
    return list(MONO_PATTERNS[0 if v < 1 / 5 else 1 if v < 2 / 5 else 2 if v < 3 / 5 else
                              3 if v < 4 / 5 else 4])


def mono_line(col):
    return 7 if _grey(col) > 0.5 else 0


def get_colour(text):
    """getColor: '#RRGGBB' -> int, '' or 'none' -> None, anything else -> green."""
    if text is None or text == '' or text == 'none':
        return None
    if text[0] == '#':
        try:
            return int(text[1:], 16) & 0xFFFFFF
        except ValueError:
            return 0x00FF00
    return 0x00FF00


def _num(v):
    m = re.match(r'\s*(-?\d*\.?\d+(?:e-?\d+)?)', v or '')
    return float(m.group(1)) if m else 0.0


def _points(text):
    pts = []
    for tok in re.split(r'\s+', text or ''):
        if not tok:
            continue
        p = tok.split(',')
        if len(p) >= 2:
            pts.append((_num(p[0]) * SCALE_X, _num(p[1]) * SCALE_Y))
    return pts


def _path(d):
    """M/L/H/V/Z (absolute and relative) -> list of (points, closed) figures."""
    figs, cur, closed = [], [], False
    x = y = sx = sy = 0.0
    toks = re.findall(r'[A-Za-z]|-?\d*\.?\d+(?:e-?\d+)?', d or '')
    i, cmd = 0, None

    def num():
        nonlocal i
        v = float(toks[i]); i += 1
        return v

    def flush():
        nonlocal cur, closed
        if len(cur) > 1:
            figs.append((cur, closed))
        cur, closed = [], False

    while i < len(toks):
        if re.match(r'[A-Za-z]', toks[i]):
            cmd = toks[i]; i += 1
            if cmd in 'zZ':
                closed = True
                x, y = sx, sy
                flush()
                continue
        if cmd is None:
            i += 1
            continue
        c = cmd
        if c in 'Mm':
            flush()
            nx, ny = num(), num()
            if c == 'm': nx += x; ny += y
            x, y, sx, sy = nx, ny, nx, ny
            cur = [(x, y)]
            cmd = 'L' if c == 'M' else 'l'
        elif c in 'Ll':
            nx, ny = num(), num()
            if c == 'l': nx += x; ny += y
            x, y = nx, ny; cur.append((x, y))
        elif c in 'Hh':
            v = num(); x = v + (x if c == 'h' else 0); cur.append((x, y))
        elif c in 'Vv':
            v = num(); y = v + (y if c == 'v' else 0); cur.append((x, y))
        else:
            raise ValueError(f"unsupported path command {c}")
    flush()
    return [([(px * SCALE_X, py * SCALE_Y) for px, py in pts], cl) for pts, cl in figs]


def _fill(img, figs, cols):
    """Even-odd polygon fill without antialiasing. Pixel centres are at integer
    coordinates (GDI+); a pixel is filled when its centre lies inside."""
    edges = []
    for pts, _closed in figs:                     # fills always close their figures
        n = len(pts)
        for k in range(n):
            (x0, y0), (x1, y1) = pts[k], pts[(k + 1) % n]
            if y0 != y1:
                edges.append((x0, y0, x1, y1) if y0 < y1 else (x1, y1, x0, y0))
    if not edges:
        return
    ymin = max(0, int(np.ceil(min(e[1] for e in edges))))
    ymax = min(H - 1, int(np.ceil(max(e[3] for e in edges))) - 1)
    tile = np.array(cols, dtype=np.uint8).reshape(2, 2)
    xs_idx = np.arange(W)
    for y in range(ymin, ymax + 1):
        xs = sorted(x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                    for x0, y0, x1, y1 in edges if y0 <= y < y1)
        row = tile[y & 1][xs_idx & 1]
        for k in range(0, len(xs) - 1, 2):
            a = max(0, int(np.ceil(xs[k])))
            b = min(W, int(np.ceil(xs[k + 1])))
            if b > a:
                img[y, a:b] = row[a:b]

def _line(img, x0, y0, x1, y1, c):
    """1-pixel line without antialiasing, drawn at x and x+1 (DoubleLine)."""
    x0, y0, x1, y1 = (int(round(v)) for v in (x0, y0, x1, y1))
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        if 0 <= y0 < H:
            if 0 <= x0 < W: img[y0, x0] = c
            if 0 <= x0 + 1 < W: img[y0, x0 + 1] = c
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy: err += dy; x0 += sx
        if e2 <= dx: err += dx; y0 += sy

def _smooth_rgb(svg_bytes, gamma=1.0, mono=False):
    """Render the source SVG with CairoSVG for an enhanced, anti-aliased image.

    This is intentionally separate from the original PC-8801 renderer.  The
    DOS image format still has an 8-colour palette, so the RGB result is
    reduced back to that palette by ``render(..., smooth=True)``.
    """
    try:
        import cairosvg
        from PIL import Image
        import io
    except ImportError as exc:
        raise RuntimeError(
            "Smooth / Enhanced rendering needs CairoSVG and Pillow. "
            "Install them with: pip install cairosvg pillow"
        ) from exc

    # Render at 2x and downsample.  This gives Cairo more information at
    # polygon edges before the result is reduced to the DOS palette.
    png = cairosvg.svg2png(bytestring=svg_bytes, output_width=W * 2, output_height=H * 2)
    image = Image.open(io.BytesIO(png)).convert("RGB")
    image = image.resize((W, H), Image.Resampling.LANCZOS)
    rgb = np.asarray(image, dtype=np.float32) / 255.0

    if gamma != 1.0:
        rgb = np.clip(rgb, 0.0, 1.0) ** (1.0 / gamma)

    # Quantize the enhanced image into the selected DOS palette.  Anti-aliased
    # edge pixels therefore never introduce unsupported colours.
    if mono:
        lum = np.sum(rgb * np.asarray([0.29891, 0.58661, 0.11448], dtype=np.float32), axis=2)
        return np.where(lum >= 0.5, 7, 0).astype(np.uint8)

    pal = np.asarray(_PAL_RGB, dtype=np.float32) / 255.0
    lum_weights = np.asarray([0.29891, 0.58661, 0.11448], dtype=np.float32)
    # Use RGB distance for the enhanced renderer; this preserves coloured edges
    # better than the historical luminance-only palette selection.
    diff = rgb[:, :, None, :] - pal[None, None, :, :]
    dist = np.sum(diff * diff, axis=3)
    return np.argmin(dist, axis=2).astype(np.uint8)



def render(svg_bytes, mono=False, gamma=1.0, smooth=False):
    """mono: two-colour rendering of the green-monitor machines; gamma: the picture's
    monochrome gamma correction (evimage/_cgprops.tjs)."""
    if smooth:
        # Enhanced mode uses a modern SVG renderer, while the normal path below
        # remains the byte-for-byte-oriented PC-8801/MZ rendering logic.
        idx = _smooth_rgb(svg_bytes, gamma, mono=mono)
        # Smooth mode is an enhanced final-image mode; there is no faithful
        # vector replay to attach to it.
        return idx, []

    s = svg_bytes.decode('utf-8', 'replace')
    s = re.sub(r'<!DOCTYPE.*?\]>', '', s, flags=re.S)
    s = re.sub(r'&ns_[a-z_]+;', 'http://ns.invalid/', s)
    s = re.sub(r'\s(i|x|graph|sfw|a):[\w-]+="[^"]*"', '', s)
    root = ET.fromstring(s)

    img = np.full((H, W), 7, dtype=np.uint8)       # white canvas
    ox = _num(root.get('x')); oy = _num(root.get('y'))

    elements = []
    for el in root.iter():
        tag = el.tag.split('}')[-1]
        if tag not in ('polygon', 'polyline', 'rect', 'line', 'path'):
            continue
        fill = None if tag == 'line' else get_colour(el.get('fill'))
        stroke = get_colour(el.get('stroke'))
        figs = []
        if tag in ('polygon', 'polyline'):
            pts = _points(el.get('points'))
            if pts:
                figs = [(pts, tag == 'polygon')]
        elif tag == 'rect':
            x = ox + _num(el.get('x')) * SCALE_X; y = oy + _num(el.get('y')) * SCALE_Y
            w = _num(el.get('width')) * SCALE_X; h = _num(el.get('height')) * SCALE_Y
            figs = [([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True)]
        elif tag == 'line':
            figs = [([(_num(el.get('x1')) * SCALE_X, _num(el.get('y1')) * SCALE_Y),
                      (_num(el.get('x2')) * SCALE_X, _num(el.get('y2')) * SCALE_Y)], False)]
        elif tag == 'path':
            figs = _path(el.get('d'))
        if not figs or (fill is None and stroke is None):
            continue

        fcols = scol = None
        if fill is not None:
            fcols = mono_tile(fill, gamma) if mono else dither_tile(fill)
            _fill(img, figs, fcols)
        if stroke is not None:
            scol = mono_line(stroke) if mono else line_colour(stroke)
            for pts, closed in figs:
                seq = pts + ([pts[0]] if closed else [])
                if len(seq) == 1:
                    _line(img, seq[0][0], seq[0][1], seq[0][0], seq[0][1], scol)
                for (ax, ay), (bx, by) in zip(seq, seq[1:]):
                    _line(img, ax, ay, bx, by, scol)
        elements.append(('fill' if fill is not None else 'line', fcols, scol, figs))

    idx = img
    return idx, elements


def to_rgb(idx, scanlines=True):
    """Preview: 640x400 RGB with the original's scanline blind (odd rows at 25%)."""
    pal = np.array(_PAL_RGB, dtype=np.uint8)
    img = pal[idx]
    out = np.repeat(img, 2, axis=0)
    if scanlines:
        out[1::2] = (out[1::2].astype(np.int32) * 63 // 255).astype(np.uint8)
    return out
