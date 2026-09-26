"""Splits the game into 1.44 MB floppies, writes INSTALL.LST for INSTALL.EXE, and
builds the floppies as folders and/or as raw FAT12 disk images (.IMA)."""
import os
import struct
import time
import zlib

SECTOR = 512

# name: (total sectors, sectors per cluster, root entries, sectors per FAT, media,
#        sectors per track, heads)
FORMATS = {
    '360K':  (720,  2, 112, 2, 0xFD, 9, 2),
    '720K':  (1440, 2, 112, 3, 0xF9, 9, 2),
    '1.2M':  (2400, 1, 224, 7, 0xF9, 15, 2),
    '1.44M': (2880, 1, 224, 9, 0xF0, 18, 2),
    '2.88M': (5760, 2, 240, 9, 0xF0, 36, 2),
}
FIRST = ['INSTALL.EXE', 'INSTALL.LST', 'INSTALL.TXT']   # disk 1, not part of the game


def geometry(fmt):
    total, spc, root, spf, media, spt, heads = FORMATS[fmt]
    data_start = 1 + 2 * spf + root * 32 // SECTOR
    clusters = (total - data_start) // spc
    return dict(total=total, spc=spc, root=root, spf=spf, media=media, spt=spt, heads=heads,
                data_start=data_start, clusters=clusters, csize=spc * SECTOR)


def piece_name(name, k):
    base, _, ext = name.partition('.')
    return f"{base[:8]}.{ext[:2]}{k}"


def plan(files, order, fmt='1.44M', compress=False, log=print):
    """files: {name: bytes}; order: names of the game files, in disk order.
    Returns a list of disks ({name: bytes}); disk 1 also holds INSTALL.* and the list."""
    import zlib
    g = geometry(fmt)
    cs = g['csize']
    rnd = lambda n: -(-n // cs) * cs
    reserve = sum(rnd(len(files[n])) for n in FIRST if n in files and n != 'INSTALL.LST') + rnd(6000)
    disks = [{}]
    free = [g['clusters'] * cs - reserve]
    entries = [g['root'] - 1 - len(FIRST)]       # root directory slots (label included)
    pieces, outs = [], []
    for name in order:
        if name not in files:
            raise ValueError(f"missing file for floppy build: {name}")
        raw = files[name]
        method = 0
        data = raw
        if compress:
            log(f"Compressing {name}...")
            import lzss
            data = lzss.compress(raw)
            method = 1
        first, pos, k = len(pieces), 0, 1
        while pos < len(data) or k == 1:
            if free[-1] < cs * 8 or entries[-1] < 1:
                disks.append({}); free.append(g['clusters'] * cs); entries.append(g['root'] - 1)
            n = min(len(data) - pos, (free[-1] // cs) * cs)
            pname = piece_name(name, k)
            chunk = data[pos:pos + n]
            disks[-1][pname] = chunk
            pieces.append((pname, chunk, len(disks)))
            free[-1] -= rnd(max(n, 1)); entries[-1] -= 1
            pos += n; k += 1
            if pos >= len(data):
                break
        outs.append((name, raw, first, len(pieces) - first, method))
    lines = [f"P {n} {len(d)} {zlib.crc32(d):08x} {k}" for n, d, k in pieces]
    lines += [f"O {n} {len(d)} {zlib.crc32(d):08x} {f} {c} {m}" for n, d, f, c, m in outs]
    # INSTALL.EXE uses fixed-size DOS arrays. Fail during conversion rather than
    # creating a floppy set that the installer cannot represent.
    if len(pieces) > 80:
        raise ValueError(f"floppy build needs {len(pieces)} pieces, but INSTALL.EXE supports at most 80")
    if len(outs) > 16:
        raise ValueError(f"floppy build has {len(outs)} output files, but INSTALL.EXE supports at most 16")
    for n in FIRST:
        if n in files:
            disks[0][n] = files[n]
    disks[0]['INSTALL.LST'] = ('\r\n'.join(lines) + '\r\n').encode('ascii')
    return disks


def _dos_name(name):
    base, _, ext = name.upper().partition('.')
    return base[:8].ljust(8).encode('ascii') + ext[:3].ljust(3).encode('ascii')


def fat12_image(files, label, fmt='1.44M'):
    """Raw FAT12 floppy image of the given format with the files in the root directory."""
    g = geometry(fmt)
    img = bytearray(g['total'] * SECTOR)
    boot = bytearray(SECTOR)
    boot[0:3] = b'\xEB\x3C\x90'
    boot[3:11] = b'SGSODOS '
    struct.pack_into('<HBHBHHBHHHII', boot, 11, SECTOR, g['spc'], 1, 2, g['root'], g['total'],
                     g['media'], g['spf'], g['spt'], g['heads'], 0, 0)
    struct.pack_into('<BBBI', boot, 36, 0, 0, 0x29, int(time.time()) & 0xFFFFFFFF)
    boot[43:54] = label.upper()[:11].ljust(11).encode('ascii')
    boot[54:62] = b'FAT12   '
    # boot code: print a message, wait for a key, then try the next boot device
    msg = b'This is a data disk (SG Space Octet DOS). Insert a system disk.\r\n\0'
    code = bytearray([0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0xFB,
                      0xBE, 0x00, 0x00,
                      0xAC, 0x08, 0xC0, 0x74, 0x09, 0xB4, 0x0E, 0xBB, 0x07, 0x00, 0xCD, 0x10,
                      0xEB, 0xF2, 0x31, 0xC0, 0xCD, 0x16, 0xCD, 0x19])
    struct.pack_into('<H', code, 12, 0x7C00 + 62 + len(code))
    boot[62:62 + len(code)] = code
    boot[62 + len(code):62 + len(code) + len(msg)] = msg
    boot[510:512] = b'\x55\xAA'
    img[0:SECTOR] = boot

    cs = g['csize']
    fat = [0] * (g['clusters'] + 2)
    fat[0], fat[1] = 0xF00 | g['media'], 0xFFF
    root = bytearray(g['root'] * 32)
    t = time.localtime()
    dtime = (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)
    ddate = ((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday
    root[0:11] = label.upper()[:11].ljust(11).encode('ascii')
    root[11] = 0x08                                     # volume label
    struct.pack_into('<HH', root, 22, dtime, ddate)
    cluster, entry = 2, 1
    for name, data in files.items():
        n = -(-len(data) // cs)
        start = cluster if n else 0
        for k in range(n):
            c = cluster + k
            fat[c] = c + 1 if k < n - 1 else 0xFFF
            off = (g['data_start'] + (c - 2) * g['spc']) * SECTOR
            img[off:off + cs] = data[k * cs:(k + 1) * cs].ljust(cs, b'\0')
        cluster += n
        e = entry * 32
        root[e:e + 11] = _dos_name(name)
        root[e + 11] = 0x20
        struct.pack_into('<HHHI', root, e + 22, dtime, ddate, start, len(data))
        entry += 1
    if cluster - 2 > g['clusters'] or entry > g['root']:
        raise ValueError(f"{label} is too full")
    fatb = bytearray(g['spf'] * SECTOR)
    fat.append(0)
    for i in range(0, len(fat) - 1, 2):
        v = fat[i] | (fat[i + 1] << 12)
        o = i * 3 // 2
        if o + 3 <= len(fatb):
            fatb[o:o + 3] = struct.pack('<I', v)[:3]
    for f in range(2):
        o = (1 + f * g['spf']) * SECTOR
        img[o:o + len(fatb)] = fatb
    o = (1 + 2 * g['spf']) * SECTOR
    img[o:o + len(root)] = root
    return bytes(img)


def write_disks(disks, outdir, folders=True, images=True, fmt='1.44M'):
    """Writes DISKn folders and/or DISKn.IMA images into outdir."""
    os.makedirs(outdir, exist_ok=True)
    for i, d in enumerate(disks, 1):
        if folders:
            p = os.path.join(outdir, f'DISK{i}')
            os.makedirs(p, exist_ok=True)
            for n, data in d.items():
                with open(os.path.join(p, n), 'wb') as f:
                    f.write(data)
        if images:
            with open(os.path.join(outdir, f'DISK{i}.IMA'), 'wb') as f:
                f.write(fat12_image(d, f'SG8 DISK {i}', fmt))
