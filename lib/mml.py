"""MML compiler (dialect of the STEINS;GATE 8bit .mus files) -> PC speaker events:
list of (PIT divisor, duration in ticks). Divisor 0 = silence."""
import re

PIT = 1193182
RATE = 240                       # player rate (ticks per second)
NOTE = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}

def freq(octave, semi):
    n = octave * 12 + semi - (4 * 12 + 9)          # o4 a = 440 Hz
    return 440.0 * 2 ** (n / 12.0)

class MMLError(Exception):
    pass

def parse_body(text):
    """Extracts the MML text per channel (lines 'X <mml>')."""
    chans = {}
    for line in text.replace('\r', '').split('\n'):
        line = line.split('//')[0]
        if not line.strip() or line.startswith('#') or line.startswith('@'):
            continue
        m = re.match(r'([A-Z]+)\s+(.*)', line)
        if not m:
            raise MMLError(f"unexpected line: {line!r}")
        for ch in m.group(1):
            chans.setdefault(ch, []).append(m.group(2))
    return chans

def expand_loops(s, default_count=2):
    """Expands [ ... / ... ]n loops (nested)."""
    def parse(i):
        out, alt = [], None
        while i < len(s):
            c = s[i]
            if c == '[':
                inner, alt_i, i = parse(i + 1)
                m = re.match(r'\d+', s[i:])
                n = int(m.group()) if m else default_count
                if m: i += len(m.group())
                for k in range(n):
                    out.append(inner)
                    if alt_i is not None and k < n - 1:
                        out.append(alt_i)
                continue
            if c == ']':
                if alt is not None:
                    return ''.join(out[:alt]), ''.join(out[alt:]), i + 1
                return ''.join(out), None, i + 1
            if c == '/':
                alt = len(out); i += 1; continue
            out.append(c); i += 1
        return ''.join(out), None, i
    # The part before '/' plays on every pass, the part after on all but the last.
    res, _, _ = parse(0)
    return res

def opl_word(f, vol):
    """OPL2 frequency word: F-number (10 bits) | block << 10 | volume (0-7) << 13."""
    for block in range(8):
        fnum = int(round(f * (1 << (20 - block)) / 49716.0))
        if fnum < 1024:
            return fnum | (block << 10) | ((min(15, max(0, vol)) >> 1) << 13)
    return 1023 | (7 << 10) | ((vol >> 1) << 13)

def compile_mml(src, transpose=0, octave_normal=True, target='pit'):
    """target 'pit': events carry PC speaker divisors; 'opl': OPL2 frequency words."""
    s = expand_loops(re.sub(r'\s+', '', src))
    s = re.sub(r'MPOF|MAOF|ROF|EAOF', '', s)
    s = re.sub(r'MP-?\d+(,-?\d+)*|EA\d+(,\d+)*|R\d+(,\d+)*|@\d+', '', s)
    tempo, deflen, octave, gate, vol = 120.0, 4, 4, 8, 15
    t = 0.0                                  # time in seconds
    raw = []                                 # (freq or 0, seconds, legato)
    loop_at = None
    i = 0
    up, down = ('>', '<') if octave_normal else ('<', '>')

    def length_at(j, base):
        m = re.match(r'(\d*)(\.*)', s[j:])
        n = int(m.group(1)) if m.group(1) else base
        d = 4 * 60.0 / tempo / n
        dots, add = len(m.group(2)), d
        for _ in range(dots):
            add /= 2; d += add
        return d, j + len(m.group())

    while i < len(s):
        c = s[i]
        if c in NOTE or c == 'r':
            i += 1
            semi = NOTE.get(c, 0)
            while i < len(s) and s[i] in '+-#=':
                semi += {'+': 1, '#': 1, '-': -1, '=': 0}[s[i]]; i += 1
            dur, i = length_at(i, deflen)
            glide_to = None
            while i < len(s) and s[i] in '^&_':
                if s[i] in '^&':
                    extra, i = length_at(i + 1, deflen)
                    dur += extra
                else:                                  # '_': portamento to the next note
                    j = i + 1
                    oc = octave
                    while s[j] in '<>':
                        oc += 1 if s[j] == up else -1; j += 1
                    if s[j] not in NOTE: raise MMLError("invalid portamento")
                    sm = NOTE[s[j]]; j += 1
                    while j < len(s) and s[j] in '+-#': sm += 1 if s[j] in '+#' else -1; j += 1
                    extra, j = length_at(j, deflen)
                    glide_to = (freq(oc + transpose, sm), extra)
                    i = j
            if c == 'r' or vol == 0:
                raw.append((0, dur, False))
            else:
                f = freq(octave + transpose, semi)
                if target == 'opl':
                    f = (f, vol)
                    glide_to = None
                if glide_to:
                    raw.append((f, dur, 'glide', glide_to))
                else:
                    on = dur * gate / 8.0
                    raw.append((f, on, gate == 8))
                    if dur - on > 1e-9: raw.append((0, dur - on, False))
            continue
        i += 1
        if c == up: octave += 1
        elif c == down: octave -= 1
        elif c == 'L': loop_at = len(raw)
        elif c in 'tloqvDP':
            m = re.match(r'-?\d+(\.\d+)?', s[i:])
            if not m: raise MMLError(f"missing parameter after {c}")
            v = float(m.group()); i += len(m.group())
            if c == 't': tempo = v
            elif c == 'l': deflen = int(v)
            elif c == 'o': octave = int(v)
            elif c == 'q': gate = int(v)
            elif c == 'v': vol = int(v)
        elif c in '()':                          # volume down / up by n (default 1)
            m = re.match(r'\d+', s[i:])
            k = int(m.group()) if m else 1
            if m: i += len(m.group())
            vol = max(0, min(15, vol - k if c == '(' else vol + k))
        elif c in ',': pass
        else:
            raise MMLError(f"unknown MML command {c!r} near {s[i-5:i+10]!r}")

    # convert to ticks without drift (accumulated in absolute time)
    ev, loop_ev, clock = [], 0, 0.0
    def push(f, secs, articulate):
        nonlocal clock
        a = round(clock * RATE); clock += secs; b = round(clock * RATE)
        n = b - a
        if n <= 0: return
        if target == 'opl':
            div = opl_word(f[0], f[1]) if f else 0
        else:
            div = int(round(PIT / f)) if f else 0
            if div > 65535: div = 0
        if articulate and n > 2:
            ev.append([div, n - 1]); ev.append([0, 1])
        else:
            ev.append([div, n])
    for k, r in enumerate(raw):
        if k == loop_at: loop_ev = len(ev)
        if len(r) == 4:                               # portamento
            f0, dur, _, (f1, gl) = r
            push(f0, dur, False)
            steps = max(1, round(gl * RATE / 2))
            for st in range(steps):
                push(f0 + (f1 - f0) * (st + 1) / steps, gl / steps, False)
        else:
            push(*r)
    # merge identical consecutive events
    out, out_loop = [], 0
    for k, (d, n) in enumerate(ev):
        if k == loop_ev: out_loop = len(out)
        if out and out[-1][0] == d and out[-1][1] + n < 65535 and k != loop_ev:
            out[-1][1] += n
        else:
            out.append([d, n])
    if loop_at is None: out_loop = None
    return out, out_loop, clock

def title_of(text):
    m = re.search(r'#title\s+"([^"]*)"', text)
    return m.group(1) if m else ''
