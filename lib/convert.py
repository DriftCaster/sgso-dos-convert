#!/usr/bin/env python3
"""Converts STEINS;GATE 8bit (EN) data.xp3 into files for SG8.EXE (DOS).
Part of SG Space Octet DOS Convert Tool, by coffee.crisp.

Usage: python3 sg8conv.py path/to/data.xp3 output_dir
Produces, following the original engine in its PC-8801 mkIISR mode:
  SG8.SCN  compiled scenarios
  SG8.IMG  pictures, 640x200 in the 8 digital colours with 2x2 dither tiles
  SG8.VEC  drawing order (strokes and fills) to replay the pictures being drawn
  SG8.FNT  the game's PC-8801 font plus the full-width glyphs used by the text
  SG8.MUS  PC speaker music (one-voice PWM arrangements)
Dependencies: pip install cairocffi pillow numpy ; IPA Gothic font (fonts-ipafont-gothic)
"""
import struct, zlib, re, sys, os, io, argparse

# ---------------------------------------------------------------- XP3
class ConvertError(Exception):
    pass

def read_xp3(data):
    """data: the bytes of data.xp3 (or a path to it)."""
    d = data if isinstance(data, (bytes, bytearray)) else open(data, 'rb').read()
    if d[:11] != b'XP3\r\n \n\x1a\x8b\x67\x01':
        raise ConvertError("invalid data.xp3")
    off = struct.unpack('<Q', d[11:19])[0]
    if d[off] == 0x80:                       # indirect index (XP3 v2)
        off = struct.unpack('<Q', d[off+9:off+17])[0]
    if d[off] & 7:
        c = struct.unpack('<Q', d[off+1:off+9])[0]
        idx = zlib.decompress(d[off+17:off+17+c])
    else:
        c = struct.unpack('<Q', d[off+1:off+9])[0]
        idx = d[off+9:off+9+c]
    files, i = {}, 0
    while i < len(idx):
        tag = idx[i:i+4]; sz = struct.unpack('<Q', idx[i+4:i+12])[0]
        body = idx[i+12:i+12+sz]; i += 12 + sz
        if tag != b'File':
            continue
        j, name, segs, flags = 0, None, [], 0
        while j < len(body):
            t = body[j:j+4]; s = struct.unpack('<Q', body[j+4:j+12])[0]
            b = body[j+12:j+12+s]; j += 12 + s
            if t == b'info':
                flags, org, arc, nl = struct.unpack('<IQQH', b[:22])
                name = b[22:22+nl*2].decode('utf-16le')
            elif t == b'segm':
                segs = [struct.unpack('<IQQQ', b[k:k+28]) for k in range(0, len(b), 28)]
        if flags & 0x80000000:
            raise ConvertError("Encrypted archive: not supported")
        out = b''.join(zlib.decompress(d[o:o+pk]) if fl & 7 else d[o:o+pk]
                       for fl, o, org, pk in segs)
        files[name.lower().replace('\\', '/')] = out
    return files

# ---------------------------------------------------------------- text
CHARMAP = {
    '♪': b'\x0d', '☆': b'*', '★': b'*', 'α': b'\xe0', 'β': b'\xe1', 'Σ': b'\xe4',
    '×': b'x', '…': b'...', '『': b'"', '』': b'"', '\u3000': b' ', '―': b'-',
    '—': b'-', '–': b'-', '‘': b"'", '’': b"'", '“': b'"', '”': b'"', '～': b'~',
    '〜': b'~', '・': b'.', '。': b'.', '、': b',',
}
def zen2han(s):
    r = []
    for ch in s:
        o = ord(ch)
        if 0xFF01 <= o <= 0xFF5E:
            ch = chr(o - 0xFEE0)
        r.append(ch)
    return ''.join(r)

def to_cp437(s, where):
    s = zen2han(s)
    out = bytearray()
    for ch in s:
        if ch in CHARMAP:
            out += CHARMAP[ch]; continue
        try:
            out += ch.encode('cp437')
        except UnicodeEncodeError:
            raise ConvertError(f"{where}: cannot convert character {ch!r} in {s!r}")
    return bytes(out)

# Lines left in Japanese by the translation (end credits)
LINE_FIX = {
    # music gallery is open from the start in the DOS demake
    'Congratulations on clearing the game.': 'Welcome to the music gallery.',
}
NAME_FIX = {'Rintaru': 'Rintaro', 'Rintaoru': 'Rintaro'}
MARK_LASTINPUT = '\x01'
MARK_VOICE = '\x02'                    # followed by '0' normal, '1' male, '2' female
# Voice classes taken from the game's macros (macro.ks): typing sound per character
VOICE = {n: '1' for n in ('Rintaro', 'Tennouji', 'Itaru', 'Neithardt', '???', 'Conductor', 'Master')}
VOICE.update({n: '2' for n in ('Kurisu', 'Mayuri', 'Luka', 'Lukako', 'Suzuha', 'Moeka', 'Faris',
                               'Nae', 'Announcement')})

def clean_text(s):
    """Applies what KAG does to a text line: [emb] of the last input and the [Name]
    character macros (typing voice + name). Other characters are displayed as written,
    full-width ones included (e.g. 「Rintaro」)."""
    s = LINE_FIX.get(s, s)
    if s.startswith('[emb exp=tf.check]'):
        return ''                                  # emulated-machine announcement: not applicable
    s = re.sub(r'\[emb\s+exp="?tf\.LastInput"?\s*\]', MARK_LASTINPUT, s)
    def name(m):
        n = NAME_FIX.get(m.group(1).strip(), m.group(1).strip())
        return MARK_VOICE + VOICE.get(n, '0') + n
    s = re.sub(r'^\[([A-Za-z?][^\]=]*)\]', name, s)
    s = s.replace('[r]', ' ')
    if '[' in s and re.search(r'\[[a-z]+[ \]]', s):
        raise ConvertError(f"unsupported tag: {s!r}")
    return s

def encode_text(s, ctx):
    """ASCII stays as is; any other character becomes 0x03 + index of a full-width glyph."""
    out = bytearray()
    for ch in s:
        o = ord(ch)
        if o < 0x80:
            out.append(o)
        else:
            out += bytes([3, ctx.zen_index(ch)])
    return bytes(out)

# ---------------------------------------------------------------- scenarios
OP = dict(TEXT=1, DRAW=3, CLEAR=4, PUSHDRAW=5, POPDRAW=6, WAIT=7, WAITCLICK=8,
          INPUT=9, PROMPT=10, SET=11, IF=12, JMP=13, GOTO=14, DONE=15,
          SAVEASK=16, LOADASK=17, SAVE=18, LOAD=19, GAMEOVER=20, RESTART=21,
          END=22, PLAY=23, STOP=24, SE=25, TITLE=26)
NS = {'f': 0, 'tf': 1, 'sf': 2}
E_VAR, E_CONST, E_EQ, E_NE, E_AND, E_OR, E_NOT, E_PREFIX = 1, 2, 3, 4, 5, 6, 7, 8

class Ctx:
    def __init__(self):
        self.vars = {0: {}, 1: {}, 2: {}}
        self.images = []
        self.zen = ['▼']                          # full-width glyphs; 0 = page-break cursor
        self.bgm = []
    def var(self, full):
        ns, name = full.split('.', 1)
        t = self.vars[NS[ns]]
        if name not in t: t[name] = len(t)
        return NS[ns], t[name]
    def zen_index(self, ch):
        if ch not in self.zen:
            self.zen.append(ch)
        if len(self.zen) > 255: raise ConvertError("too many full-width characters")
        return self.zen.index(ch)
    def image(self, name):
        name = name.lower()
        if name not in self.images: self.images.append(name)
        return self.images.index(name)
    def music(self, name):
        name = name.lower()
        if name not in self.bgm: self.bgm.append(name)
        return self.bgm.index(name)

def attrs(s):
    r = {}
    for m in re.finditer(r'(\w+)(?:\s*=\s*("[^"]*"|\'[^\']*\'|\S+))?', s):
        v = m.group(2)
        if v is None: v = True
        elif v[0] in '"\'': v = v[1:-1]
        r[m.group(1)] = v
    return r

def compile_expr(ctx, e, where):
    e = e.strip()
    if e.startswith('((string)tf.LastInput).toLowerCase().substr(0,'):
        m = re.search(r"=='([^']*)'", e)
        p = m.group(1).encode()
        return bytes([E_PREFIX, len(p)]) + p
    if e == 'kag.SG8bit.kanjiEnabled':
        return bytes([E_CONST]) + struct.pack('<h', 1)
    for flag in UNLOCKED_FLAGS:
        if re.fullmatch(re.escape(flag) + r'\s*==\s*1', e):
            return bytes([E_CONST]) + struct.pack('<h', 1)
    out = b''
    ors = [x for x in re.split(r'\|\|', e)]
    for oi, o in enumerate(ors):
        ands = re.split(r'&&', o)
        for ai, a in enumerate(ands):
            a = a.strip()
            m = re.fullmatch(r"((?:sf|tf|f)\.[^\s=!]+)\s*(==|!=)\s*(-?\d+|'')", a)
            if m:
                ns, idx = ctx.var(m.group(1))
                v = 0 if m.group(3) == "''" else int(m.group(3))
                out += bytes([E_VAR, ns]) + struct.pack('<H', idx)
                out += bytes([E_CONST]) + struct.pack('<h', v)
                out += bytes([E_EQ if m.group(2) == '==' else E_NE])
            else:
                m = re.fullmatch(r"(!?)((?:sf|tf|f)\.[^\s=!]+)", a)
                if not m: raise ConvertError(f"{where}: unsupported expression {e!r}")
                ns, idx = ctx.var(m.group(2))
                out += bytes([E_VAR, ns]) + struct.pack('<H', idx)
                if m.group(1): out += bytes([E_NOT])
            if ai: out += bytes([E_AND])
        if oi: out += bytes([E_OR])
    return out

def pstr(s, where):
    b = to_cp437(s, where)
    return bytes([len(b)]) + b

class ScnCompiler:
    def __init__(self, ctx, name, text, scnnames):
        self.ctx, self.name, self.scnnames = ctx, name, scnnames
        self.code = bytearray(); self.labels = []; self.ifstack = []
        self.lines = text.split('\n')
    def emit(self, *parts):
        for p in parts:
            self.code += p if isinstance(p, (bytes, bytearray)) else bytes([p])
    def text(self, s, where):
        b = encode_text(s, self.ctx)
        self.emit(OP['TEXT'], struct.pack('<H', len(b)), b)
    def run(self):
        pend = ''
        for ln, raw in enumerate(self.lines, 1):
            where = f"{self.name}:{ln}"
            line = raw.rstrip('\r').strip()
            if not line or line.startswith(';') or line.startswith('//'):
                continue
            if line.startswith('*'):
                lab = line[1:].split('|')[0].strip().lower()
                lab = re.sub(r'\s+', ' ', lab)
                lab = '*' if line.startswith('**') else lab
                self.labels.append((lab, len(self.code)))
                continue
            if line.startswith('@'):
                self.command(line[1:], where)
                continue
            orig = line
            line = clean_text(line)
            if line.strip():
                self.text(line.strip(), where)
            for extra in TEXT_AFTER.get((self.name, orig), []):
                self.text(extra, where)
        end = len(self.code)
        self.emit(OP['END'])
        while self.ifstack:                      # unclosed @if in the original script
            pass                                    # unclosed @if in the original script
            top = self.ifstack.pop()
            if top[0] is not None: self.patch(top[0], end)
            for q in top[1]: self.patch(q, end)
        if len(self.code) > 60000: raise ConvertError(f"{self.name}: too large")
    def patch(self, pos, val):
        self.code[pos:pos+2] = struct.pack('<H', val)
    def command(self, s, where):
        m = re.match(r'(\w+)\s*(.*)', s)
        cmd, a = m.group(1).lower(), attrs(m.group(2))
        c = self.ctx
        if cmd in ('draw', 'redraw', 'pushdraw'):
            self.emit(OP['PUSHDRAW' if cmd == 'pushdraw' else 'DRAW'],
                      struct.pack('<H', c.image(a['file'])))
        elif cmd == 'popdraw': self.emit(OP['POPDRAW'])
        elif cmd == 'clear': self.emit(OP['CLEAR'])
        elif cmd == 'wait': self.emit(OP['WAIT'], struct.pack('<H', int(a['time'])))
        elif cmd == 'waitclick': self.emit(OP['WAITCLICK'])
        elif cmd == 'input': self.emit(OP['INPUT'], 0)
        elif cmd == 'title_input': self.emit(OP['INPUT'], 1, pstr('start/load/help? ', where))
        elif cmd == 'extra_input': self.emit(OP['INPUT'], 1, pstr('select(1-18)? ', where))
        elif cmd == 'prompt':
            t = a.get('text', '')
            self.emit(OP['PROMPT'], pstr(t if isinstance(t, str) else '', where))
        elif cmd == 'set':
            name, val = a['name'], a['value']
            if name == 'tf.check': return          # emulated-machine unlock: not applicable
            ns, idx = c.var(name)
            self.emit(OP['SET'], ns, struct.pack('<Hh', idx, int(val)))
        elif cmd == 'if':
            ex = compile_expr(c, a['exp'], where)
            self.emit(OP['IF'], len(ex), ex)
            self.ifstack.append([len(self.code), []]); self.emit(b'\0\0')
        elif cmd in ('else', 'endif') and not self.ifstack:
            pass                                    # stray @else/@endif in the original script
        elif cmd == 'else':
            top = self.ifstack[-1]
            self.emit(OP['JMP']); top[1].append(len(self.code)); self.emit(b'\0\0')
            self.patch(top[0], len(self.code)); top[0] = None
        elif cmd == 'endif':
            top = self.ifstack.pop()
            if top[0] is not None: self.patch(top[0], len(self.code))
            for p in top[1]: self.patch(p, len(self.code))
        elif cmd == 'goto':
            f = os.path.splitext(a['file'].lower())[0]
            if f not in self.scnnames: raise ConvertError(f"{where}: unknown scenario {f}")
            self.emit(OP['GOTO'], struct.pack('<H', self.scnnames.index(f)))
        elif cmd == 'done': self.emit(OP['DONE'])
        elif cmd == 'eval':
            if a['exp'] == 'tf.saveload=true': self.emit(OP['SAVEASK'])
            elif a['exp'] == 'tf.saveload=false': self.emit(OP['LOADASK'])
            else: raise ConvertError(f"{where}: unsupported @eval")
        elif cmd == 'next': pass                    # follows SAVEASK/LOADASK
        elif cmd == 'game_save': self.emit(OP['SAVE'], int(a['slot']))
        elif cmd == 'game_load': self.emit(OP['LOAD'], int(a['slot']))
        elif cmd == 'gameover': self.emit(OP['GAMEOVER'], struct.pack('<H', c.image('cg67')))
        elif cmd == 'restart': self.emit(OP['RESTART'])
        elif cmd == 'play':
            self.emit(OP['PLAY'], c.music(a['file']))
            if a.get('showtitle'):
                self.emit(OP['TITLE'], c.music(a['file']))
        elif cmd == 'stop': self.emit(OP['STOP'])
        elif cmd == 'se': self.emit(OP['SE'], c.music(a['file']))
        elif cmd in ('chse', 'hackbgm', 'setmachine', 'cload', 'msghide'): pass
        else: raise ConvertError(f"{where}: unknown command @{cmd}")
    def blob(self):
        hdr = bytearray(struct.pack('<H', len(self.labels)))
        for lab, off in self.labels:
            b = to_cp437(lab, self.name)[:255]
            hdr += struct.pack('<HB', off, len(b)) + b
        codeoff = 2 + len(hdr)
        return struct.pack('<H', codeoff) + bytes(hdr) + bytes(self.code)

SKIP_SCN = {'readmefirst', 'taikenban_ed'}

# DOS demake: the music gallery is open from the start (instead of after finishing the game).
UNLOCKED_FLAGS = {'sf.クリアフラグ'}

# Lines added to the title screen help for features specific to the DOS demake.
TEXT_AFTER = {
    ('sg0', 'To save during the game, enter "Save", and choose a slot.'): [
        'To listen to the music, enter "Music".',
        'To return to this screen during the game, enter "Menu".',
        'To adjust the volume, enter "Volume" or press F9.',
    ],
}

def build_scn(files, ctx):
    names = sorted(os.path.splitext(n.split('/')[-1])[0]
                   for n in files if n.startswith('scenario/') and n.endswith('.txt'))
    names = [n for n in names if n not in SKIP_SCN]
    for need in ('sg0', 'syscmd', 'extramode'):
        assert need in names
    blobs = []
    for n in names:
        txt = files[f'scenario/{n}.txt'].decode('cp932', errors='replace')
        comp = ScnCompiler(ctx, n, txt, names); comp.run()
        blobs.append(comp.blob())
    out = bytearray(b'SG8S')
    out += struct.pack('<HHHH', len(names), names.index('sg0'),
                       names.index('syscmd'), names.index('extramode'))
    out += struct.pack('<HHH', *(len(ctx.vars[i]) for i in range(3)))
    base = len(out) + len(names) * 20
    for n, b in zip(names, blobs):
        nb = n.encode()[:12].ljust(12, b'\0')
        out += nb + struct.pack('<II', base, len(b)); base += len(b)
    for b in blobs: out += b
    return bytes(out), names

# ---------------------------------------------------------------- images
def packbits(data):
    out = bytearray(); i, n = 0, len(data)
    while i < n:
        j = i
        while j + 1 < n and data[j + 1] == data[i] and j - i < 127: j += 1
        if j > i:
            out += bytes([257 - (j - i + 1), data[i]]); i = j + 1; continue
        j = i
        while j < n and j - i < 128 and not (j + 2 < n and data[j] == data[j+1] == data[j+2]): j += 1
        out += bytes([j - i - 1]) + data[i:j]; i = j
    return bytes(out)

def planar3_rle(idx):
    """640x200 colour indices 0-7 -> per row, 3 bit planes of 80 bytes, PackBits."""
    import numpy as np
    planes = [np.packbits(((idx >> p) & 1).astype(np.uint8), axis=1) for p in range(3)]
    out = bytearray()
    for y in range(idx.shape[0]):
        for p in range(3):
            out += packbits(planes[p][y].tobytes())
    return bytes(out)

def parse_gamma(files):
    g = {}
    t = files.get('evimage/_cgprops.tjs', b'').decode('cp932', 'replace')
    for m in re.finditer(r'(\w+):\s*%\[\s*gamma:([\d.]+)', t):
        g[m.group(1).lower()] = float(m.group(2))
    return g

def build_images(files, ctx, log, mono=False):
    """SG8.IMG (final pictures) and SG8.VEC (element order for the drawing replay),
    both rendered like the original engine: PC-8801 colours, or the two-colour
    green-monitor mode with each picture's gamma correction (see pc88draw.py)."""
    import pc88draw
    gam = parse_gamma(files)
    imgs, vecs = [], []
    for i, n in enumerate(ctx.images):
        src = files.get(f'evimage/{n}.svg')
        if src is None:
            log(f"  missing image skipped: {n}"); imgs.append(b''); vecs.append(b''); continue
        idx, els = pc88draw.render(src, mono, gam.get(n, 1.0))
        imgs.append(planar3_rle(idx))
        out = bytearray()
        for kind, fcols, scol, figs in els:
            polys = [pts + ([pts[0]] if closed and kind == 'line' else []) for pts, closed in figs]
            if kind == 'line':
                data, cnt = encode_polys(polys, False)
                if cnt: out += struct.pack('<BB', 1, scol) + data
            else:
                data, cnt = encode_polys(polys, True)
                if cnt: out += struct.pack('<BB', 2, 0xFF if scol is None else scol) + data
        out += b'\0'
        vecs.append(bytes(out))
        log(f"  [{i+1}/{len(ctx.images)}] {n}")
    def pack(magic, blobs, entry8=False):
        # IMG (SG8J): engine reads two u32s per entry (offset, size)
        # VEC (SG8B): engine reads one u32 per entry (offset only)
        es = 8 if entry8 else 4
        res = bytearray(magic) + struct.pack('<H', len(blobs))
        base = len(res) + es * len(blobs)
        for b in blobs:
            if entry8:
                res += struct.pack('<II', base if b else 0, len(b) if b else 0)
            else:
                res += struct.pack('<I', base if b else 0)
            base += len(b)
        for b in blobs:
            res += b
        return bytes(res)
    return pack(b'SG8J', imgs, entry8=True), pack(b'SG8B', vecs, entry8=False)

def encode_polys(polys, closed):
    out = bytearray()
    good = []
    for p in polys:
        pts = []
        for x, y in p:
            x = max(-30000, min(30000, int(round(x)))); y = max(-30000, min(30000, int(round(y))))
            if not pts or pts[-1] != (x, y): pts.append((x, y))
        if closed and len(pts) > 1 and pts[0] == pts[-1]: pts.pop()
        if len(pts) >= (3 if closed else 2) or (not closed and len(pts) == 1):
            good.append(pts)
    out += struct.pack('<H', len(good))
    for pts in good:
        out += struct.pack('<Hhh', len(pts), *pts[0])
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            dx, dy = bx - ax, by - ay
            if -127 <= dx <= 127 and -127 <= dy <= 127:
                out += struct.pack('<bb', dx, dy)
            else:
                out += struct.pack('<bhh', -128, bx, by)
    return out, len(good)

# ---------------------------------------------------------------- font
IPA_GOTHIC = ['/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf',
              '/usr/share/fonts/truetype/fonts-japanese-gothic.ttf']

def tft_load(data):
    """KiriKiri pre-rendered font (.tft): {char: (w, h, pixels 0-64)}."""
    cnt, cio, io = struct.unpack('<III', data[24:36])
    chars = struct.unpack('<%dH' % cnt, data[cio:cio + cnt * 2])
    glyphs = {}
    for i, ch in enumerate(chars):
        off, w, h = struct.unpack('<IHH', data[io + i * 20:io + i * 20 + 8])
        px, p = [], off
        while len(px) < w * h:
            v = data[p]; p += 1
            if v >= 0x41: px += [px[-1]] * (v - 0x40)
            else: px.append(v)
        glyphs[chr(ch)] = (w, h, px[:w * h])
    return glyphs

def zen_glyph(ch, log=print):
    """drawZenChar: full-width character drawn at 16px (twice, one pixel apart vertically),
    squeezed to 8 rows then stretched back to 16 (the PC-8801 1x2 look). 16x16 bits.
    Uses the pre-rendered table (IPA Gothic); other characters are rendered with
    Pillow and IPA Gothic when available, else left blank."""
    import zenglyphs
    if ch in zenglyphs.GLYPHS:
        h = zenglyphs.GLYPHS[ch]
        return [int(h[i:i + 4], 16) for i in range(0, 64, 4)]
    try:
        from PIL import Image, ImageDraw, ImageFont
        path = next((f for f in IPA_GOTHIC if os.path.exists(f)), None)
        if path is None: raise ImportError
    except ImportError:
        log(f"  no glyph for {ch!r}: shown as a blank")
        return [0] * 16
    font = ImageFont.truetype(path, 16)
    im = Image.new('1', (16, 17), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    # IPA Gothic sits one row lower than MS Gothic in a 16px cell
    d.text((0, -1), ch, font=font, fill=1)
    d.text((0, 0), ch, font=font, fill=1)
    px = im.load()
    rows = []
    for y in range(8):
        bits = 0
        for x in range(16):
            if px[x, y * 2]: bits |= 0x8000 >> x
        rows += [bits, bits]
    return rows

def build_font(files, ctx, log=print):
    """SG8.FNT: the game's PC-8801 font (font_han1x2z.tft) for ASCII, plus full-width glyphs."""
    g = tft_load(files['image/font_han1x2z.tft'])
    out = bytearray(b'SG8F')
    for c in range(0x20, 0x7F):
        w, h, px = g.get(chr(c), (8, 16, [0] * 128))
        for y in range(16):
            bits = 0
            for x in range(min(w, 8)):
                if y < h and px[y * w + x] > 32: bits |= 0x80 >> x
            out.append(bits)
    out += struct.pack('<H', len(ctx.zen))
    for ch in ctx.zen:
        if ch in g and g[ch][0] == 16:
            w, h, px = g[ch]
            rows = [sum(0x8000 >> x for x in range(16) if y < h and px[y * 16 + x] > 32) for y in range(16)]
        else:
            rows = zen_glyph(ch, log)
        for r in rows: out += struct.pack('>H', r)
    return bytes(out)

# ---------------------------------------------------------------- music
def se_impact():
    """SE01 "Impact": originally a low noise voice, remade as a falling sweep."""
    import random
    rnd = random.Random(1)
    ev = []
    for k in range(60):                       # 250 ms from 1500 to 150 Hz
        f = 1500 * (0.1 ** (k / 59))
        ev.append([int(PIT_CLK / f), 1])
    for k in range(24):                       # noisy tail
        ev.append([int(PIT_CLK / rnd.uniform(90, 400)), 1])
    return ev

PIT_CLK = 1193182

def noisify(ev, seed):
    import random
    rnd = random.Random(seed)
    out = []
    for d, n in ev:
        for _ in range(n):
            out.append([min(65535, int(d * rnd.uniform(0.7, 1.4))) if d else 0, 1])
    return out

def pit_to_opl(ev):
    import mml
    return [[mml.opl_word(PIT_CLK / d, 14) if d else 0, n] for d, n in ev]

def build_mus(files, ctx, log):
    """SG8.MUS: for each song or effect, a title and 4 tracks of (value, ticks) events.
    Track 0: PC speaker (one-voice PWM arrangement, PIT divisors).
    Tracks 1-3: AdLib/OPL2 (the three PSG channels of the PC-8801 arrangement)."""
    import mml
    entries = []
    for name in ctx.bgm:
        title, tracks = '', [([], None)] * 4
        if name == 'se01':
            sp = se_impact()
            tracks = [(sp, None), (pit_to_opl(sp), None), ([], None), ([], None)]
        elif name.startswith('se'):
            text = files[f'sound/{name}_psg.mus'].decode('cp932', 'replace')
            ch = mml.parse_body(text)
            ev, _, _ = mml.compile_mml(''.join(ch.get('E') or ch.get('F')), transpose=3)
            sp = noisify(ev, 2)
            tracks = [(sp, None), (pit_to_opl(sp), None), ([], None), ([], None)]
        else:
            text = files[f'bgm/chama/{name}_pwm.mus'].decode('cp932', 'replace')
            sp, loop, _ = mml.compile_mml(''.join(mml.parse_body(text)['F']))
            title = mml.title_of(text).replace(' PWM', '')
            tracks = [(sp, loop)]
            psg = files.get(f'bgm/{name}_psg.mus')
            chans = mml.parse_body(psg.decode('cp932', 'replace')) if psg else {}
            normal = psg is not None and '#octave x' not in psg.decode('cp932', 'replace')
            for c in 'DEF':
                if c in chans:
                    ev, lp, _ = mml.compile_mml(''.join(chans[c]), octave_normal=normal, target='opl')
                    tracks.append((ev, lp))
                else:
                    tracks.append(([], None))
        entries.append((title, tracks))
    out = bytearray(b'SG8N') + struct.pack('<H', len(entries))
    base = len(out) + len(entries) * 80
    data = bytearray()
    for title, tracks in entries:
        out += to_cp437(title, 'title')[:47].ljust(48, b'\0')
        for ev, loop in tracks:
            if len(ev) * 4 > 32000: raise ConvertError("music track too long")
            out += struct.pack('<IHH', base + len(data), len(ev), 0xFFFF if loop is None else loop)
            for d, n in ev: data += struct.pack('<HH', d, n)
    return bytes(out + data)

def convert_all(xp3_data, log=print, progress=None, mono=False):
    """Converts data.xp3 into the SG8 data files. Returns {file name: bytes}."""
    files = read_xp3(xp3_data)
    if 'image/font_han1x2z.tft' not in files or 'scenario/sg0.txt' not in files:
        raise ConvertError("this data.xp3 is not STEINS;GATE 8bit")
    ctx = Ctx()
    out = {}
    log("Compiling scenarios...")
    out['SG8.SCN'], names = build_scn(files, ctx)
    log("Converting music...")
    out['SG8.MUS'] = build_mus(files, ctx, log)
    log("Building the font...")
    out['SG8.FNT'] = build_font(files, ctx, log)
    log(f"Drawing {len(ctx.images)} pictures...")
    out['SG8.IMG'], out['SG8.VEC'] = build_images(files, ctx, progress or (lambda m: None), mono)
    return out
