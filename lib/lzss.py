"""LZSS compression decoded by INSTALL.EXE on DOS.

Stream: groups of 8 items preceded by a flag byte (bit 0 first; 1 = literal byte,
0 = match). A match is 2 bytes: low 8 bits of (distance - 1), then
((distance - 1) >> 8) << 4 | (length - 3). Distance 1-4096, length 3-18.
"""
WINDOW = 4096
MIN_LEN, MAX_LEN = 3, 18
CHAIN = 24                           # candidates tried per position


def compress(data, progress=None):
    n = len(data)
    out = bytearray()
    heads = {}                       # 3-byte key -> list of recent positions
    i = 0
    flag_pos, flag, bit = 0, 0, 8
    step = max(1, n // 50)
    next_report = step
    while i < n:
        if bit == 8:
            if flag_pos < len(out):
                out[flag_pos] = flag
            flag_pos = len(out); out.append(0); flag, bit = 0, 0
        best_len, best_dist = 0, 0
        if i + MIN_LEN <= n:
            key = data[i:i + 3]
            cands = heads.get(key)
            if cands:
                limit = min(MAX_LEN, n - i)
                for p in reversed(cands):
                    d = i - p
                    if d > WINDOW:
                        break
                    l = 3
                    while l < limit and data[p + l] == data[i + l]:
                        l += 1
                    if l > best_len:
                        best_len, best_dist = l, d
                        if l == limit:
                            break
        if best_len >= MIN_LEN:
            d = best_dist - 1
            out.append(d & 0xFF)
            out.append(((d >> 8) << 4) | (best_len - MIN_LEN))
            end = i + best_len
        else:
            flag |= 1 << bit
            out.append(data[i])
            end = i + 1
        bit += 1
        while i < end:                # index every position covered
            if i + 3 <= n:
                k = data[i:i + 3]
                lst = heads.get(k)
                if lst is None:
                    heads[k] = [i]
                else:
                    lst.append(i)
                    if len(lst) > CHAIN:
                        del lst[0]
            i += 1
        if progress and i >= next_report:
            progress(i / n); next_report += step
    if flag_pos < len(out):
        out[flag_pos] = flag
    return bytes(out)


def decompress(data, size):
    """Decompress *data* and return exactly *size* bytes.

    The DOS installer treats truncated/corrupt compressed pieces as a data error.
    Raising ValueError here gives the Python-side tools the same kind of useful
    failure instead of leaking an IndexError from malformed input.
    """
    if size < 0:
        raise ValueError("decompression size cannot be negative")
    out = bytearray()
    i = 0

    def need(count):
        if i + count > len(data):
            raise ValueError("truncated LZSS stream")

    while len(out) < size:
        need(1)
        flag = data[i]
        i += 1
        for b in range(8):
            if len(out) >= size:
                break
            if flag & (1 << b):
                need(1)
                out.append(data[i])
                i += 1
            else:
                need(2)
                lo, hi = data[i], data[i + 1]
                i += 2
                d = (lo | ((hi >> 4) << 8)) + 1
                length = (hi & 15) + MIN_LEN
                if d > len(out):
                    raise ValueError("invalid LZSS back-reference")
                for _ in range(length):
                    if len(out) >= size:
                        break
                    out.append(out[-d])
    return bytes(out)
