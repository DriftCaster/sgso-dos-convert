#!/usr/bin/env python3
"""SG Space Octet DOS Convert Tool, by coffee.crisp.

Turns your own copy of STEINS;GATE 8bit (English) into a DOS version for real PCs
(8086+, VGA), split over floppy disks.

Window:        python sgso_convert.py
Command line:  python sgso_convert.py GAME OUTPUT [options]   (see --help)

GAME is the game's data.xp3, its folder, or a .zip containing it.
Requires Python 3.8+ and numpy (pip install numpy).
"""
import argparse
import io
import os
import queue
import sys
import threading
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))     # folder, or the .pyz file itself
sys.path.insert(0, os.path.join(HERE, 'lib'))


def resource(rel):
    """Reads a bundled file (e.g. 'dos/SG8.EXE') from the folder or from the .pyz.
    File names are matched in any letter case; DOS programs are also accepted next to
    sgso_convert.py."""
    folder, name = os.path.split(rel)
    for d in (os.path.join(HERE, folder), HERE):
        if os.path.isdir(d):
            for f in os.listdir(d):
                if f.upper() == name.upper():
                    with open(os.path.join(d, f), 'rb') as fh:
                        return fh.read()
    if zipfile.is_zipfile(HERE):
        with zipfile.ZipFile(HERE) as z:
            for n in z.namelist():
                if n.upper() == rel.upper():
                    return z.read(n)
    raise FileNotFoundError(
        f"{name} is missing: it must be in the '{folder}' folder next to sgso_convert.py "
        f"({os.path.join(HERE, folder)}). The 'dos' folder must contain SG8.EXE, SETUP.EXE "
        "and INSTALL.EXE. If they were there before, an antivirus may have removed them: "
        "restore them or add an exception for this folder.")


def check_install():
    """The program needs its lib and dos folders next to it."""
    try:
        import convert, disks          # noqa: F401
        resource('dos/SG8.EXE')
        return None
    except FileNotFoundError as e:
        return str(e)
    except (ImportError, KeyError):
        return ("The tool is incomplete: the folders 'lib' and 'dos' must be next to sgso_convert.py.\n"
                "Extract the WHOLE zip first (right-click > Extract All on Windows), then run\n"
                "sgso_convert.bat (Windows) or sgso_convert.sh (Linux) from the extracted folder,\n"
                "or use the single-file sgso_convert.pyz instead.")

APP_NAME = 'SG Space Octet DOS Convert Tool'
VERSION = '1.3'

# Build choices: (key, label, [(value, choice label)], default index)
BUILD = [
    ('pictures', 'Picture palette', [('colour', 'Colour (8-colour PC-8801 palette)'),
                              ('mono', 'Monochrome (MZ-2000 green screen)')], 0),
    ('rendering', 'Picture rendering', [('authentic', 'Authentic / faithful original renderer'),
                                        ('smooth', 'Smooth / Enhanced / anti-aliased')], 0),
    ('floppy', 'Floppy type', [('1.44M', '3.5" 1.44 MB (HD)'), ('720K', '3.5" 720 KB (DD)'),
                               ('1.2M', '5.25" 1.2 MB (HD)'), ('360K', '5.25" 360 KB (DD)'),
                               ('2.88M', '3.5" 2.88 MB (ED)')], 0),
    ('compress', 'Compression', [('on', 'On (fewer disks, INSTALL expands)'), ('off', 'Off')], 0),
    ('size', 'Drawing data', [('full', 'Full: pictures drawn as the original'),
                              ('light', 'Light: pictures appear at once')], 0),
]

# Settings written to SG8.CFG: (key, label, [(value, choice label)], default index)
SETTINGS = [
    ('screen', 'Screen', [('auto', 'Automatic'), ('colour', 'Colour'), ('grey', 'Grey (plasma, LCD)'),
                          ('green', 'Green monitor'), ('amber', 'Amber monitor')], 0),
    ('scanlines', 'Scanlines', [('on', 'On (as the original)'), ('off', 'Off')], 0),
    ('draw', 'Picture drawing', [('0', 'Instant'), ('1', 'Fast'), ('2', 'PC-8801 (original)'),
                                 ('3', 'Slow machines')], 2),
    ('textspeed', 'Text speed', [('0', 'Instant'), ('10', 'Fast'), ('25', 'Normal (original)'),
                                 ('50', 'Slow')], 2),
    ('typing', 'Typing beep', [('on', 'On'), ('off', 'Off')], 0),
    ('sound', 'Sound', [('speaker', 'PC speaker'), ('piezo', 'Piezo beeper (1 octave up)'),
                        ('adlib', 'AdLib / Sound Blaster'), ('none', 'None')], 0),
    ('music', 'Music', [('on', 'On'), ('off', 'Off')], 0),
    ('volmusic', 'Music volume', [('2', 'Loud'), ('1', 'Soft'), ('0', 'Off')], 0),
    ('voltyping', 'Typing volume', [('2', 'Loud'), ('1', 'Soft'), ('0', 'Off')], 0),
    ('voleffects', 'Effects volume', [('2', 'Loud'), ('1', 'Soft'), ('0', 'Off')], 0),
]

README = """SG Space Octet DOS
by coffee.crisp
=================

STEINS;GATE 8bit, running on DOS the way it looked in its PC-8801 mode:
8 colour dithered pictures with scanlines (or the green screen look), the
game's own PC-8801 font, the 3 line text window and the function key bar.
Music plays on the PC speaker or an AdLib / Sound Blaster.

You need an 8086 or better, DOS 3.3+, VGA, about 420 KB of free
conventional memory and about {size} MB of disk space.

  SETUP     screen, sound and speed settings (saved in SG8.CFG)
  SG8       start the game

How to play: type what you want to do in English (look, talk nae, front,
save, load, help...) and press Enter. Every line of text waits for a key.
  F1-F5        left/back/front/right/phone
  Shift+F1-F5  load/save/look/talk/assistant
  Up arrow     repeat the last command      Esc   clear the line
  F9           volume                       F10   quit
  menu         back to the title screen
  music        music gallery (from the title screen)

Command line options (they override SG8.CFG):
  /V:n picture drawing 0-3    /C:n milliseconds per letter   /N no typing beep
  /M no music   /Q no sound   /ADLIB   /SPEAKER   /O:n raise the speaker pitch
  /FLAT no scanlines   /GREY /COLOUR /GREEN /AMBER

Fan project, not affiliated with MAGES. or 5pb. No game data is included:
these files were made from your own copy of the game.
"""

INSTALL = """SG Space Octet DOS - installation
=================================

This set has {n} disk(s) of {fmt}. {notes}

On the DOS computer:
  C:
  MD \\SG8DOS
  COPY A:*.* C:\\SG8DOS          (do this for every disk)
  CD \\SG8DOS
  INSTALL                       (checks the files and puts the game together)
  SETUP                         (optional: screen and sound)
  SG8

If INSTALL says a disk is bad, copy that disk again and run INSTALL again.
Floppies written on a USB drive sometimes don't read well on old drives:
formatting them on the old PC first (FORMAT A:) usually fixes it.
"""


def find_xp3(path):
    """Returns the bytes of data.xp3 from a file, a folder or a .zip."""
    if os.path.isdir(path):
        for root, _dirs, names in os.walk(path):
            for n in names:
                if n.lower() == 'data.xp3':
                    with open(os.path.join(root, n), 'rb') as f:
                        return f.read()
        raise FileNotFoundError("no data.xp3 found in this folder")
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for n in z.namelist():
                if n.replace('\\', '/').split('/')[-1].lower() == 'data.xp3':
                    return z.read(n)
        raise FileNotFoundError("no data.xp3 found in this .zip")
    with open(path, 'rb') as f:
        return f.read()


def make_cfg(c):
    lines = []
    for key, _l, opts, d in SETTINGS:
        v = c.get(key, opts[d][0])
        if key == 'screen' and v == 'auto' and c.get('pictures') == 'mono':
            v = 'green'
        if key == 'sound':
            lines.append(f"sound={'speaker' if v == 'piezo' else v}")
            lines.append(f"octave={1 if v == 'piezo' else 0}")
        else:
            lines.append(f"{key}={v}")
    return ('\r\n'.join(lines) + '\r\n').encode('ascii')


GAME_ORDER = ['SG8.IMG', 'SG8.VEC', 'SG8.EXE', 'SETUP.EXE', 'SG8.SCN', 'SG8.MUS', 'SG8.FNT',
              'SG8.CFG', 'README.TXT']


def run(game, outdir, choices, folders=True, images=True, harddisk=True, log=print, progress=None):
    """choices: BUILD and SETTINGS values by key."""
    import convert
    import disks
    c = {k: choices.get(k, o[d][0]) for k, _l, o, d in BUILD + SETTINGS}
    log(f"{APP_NAME} {VERSION} - reading {game}")
    data = find_xp3(game)
    files = convert.convert_all(data, log, progress, mono=c['pictures'] == 'mono', smooth=c['rendering'] == 'smooth')
    for n in ('SG8.EXE', 'SETUP.EXE', 'INSTALL.EXE'):
        files[n] = resource('dos/' + n)
    if c['size'] == 'light' or c['rendering'] == 'smooth':
        # Enhanced rendering is a final anti-aliased image, not a replay of the
        # original vector drawing.  Do not pair it with the authentic draw-data.
        files.pop('SG8.VEC', None)
    files['SG8.CFG'] = make_cfg(c)
    size = sum(len(files[n]) for n in GAME_ORDER if n in files) / 1e6
    files['README.TXT'] = README.format(size=round(size + 0.5)).replace('\n', '\r\n').encode('ascii')
    order = [n for n in GAME_ORDER if n in files]
    fmt = c['floppy']
    note = []
    if c['size'] == 'light': note.append("Light set: pictures appear at once.")
    if c['pictures'] == 'mono': note.append("Monochrome pictures (green-screen mode).")
    if c['rendering'] == 'smooth':
        note.append("Smooth / Enhanced pictures: anti-aliased rendering, reduced to the selected DOS palette; not the original renderer.")
        note.append("Enhanced mode shows pictures at once; drawing replay is disabled.")
    os.makedirs(outdir, exist_ok=True)
    if folders or images:
        files['INSTALL.TXT'] = b''
        plan = disks.plan(files, order, fmt, c['compress'] == 'on', log)
        files['INSTALL.TXT'] = INSTALL.format(n=len(plan), fmt=fmt, notes=' '.join(note)) \
            .replace('\n', '\r\n').encode('ascii')
        plan[0]['INSTALL.TXT'] = files['INSTALL.TXT']
        log(f"Writing {len(plan)} {fmt} floppy disk(s)...")
        disks.write_disks(plan, os.path.join(outdir, 'FLOPPIES'), folders, images, fmt)
    else:
        plan = []
    if harddisk:
        log("Writing the ready-to-play folder SG8DOS (hard disk, DOSBox)...")
        hd = os.path.join(outdir, 'SG8DOS')
        os.makedirs(hd, exist_ok=True)
        for n in order:
            with open(os.path.join(hd, n), 'wb') as f:
                f.write(files[n])
    log(f"Done: {outdir}")
    return len(plan)


# ------------------------------------------------------------------ command line
def cli(argv):
    ap = argparse.ArgumentParser(description="Convert STEINS;GATE 8bit into a DOS version on floppies.")
    ap.add_argument('game', help="data.xp3, the game folder, or a .zip containing it")
    ap.add_argument('output', help="output folder")
    ap.add_argument('--no-folders', action='store_true', help="no DISKn folders")
    ap.add_argument('--no-images', action='store_true', help="no DISKn.IMA disk images")
    ap.add_argument('--no-harddisk', action='store_true', help="no SG8DOS folder")
    for key, label, opts, d in BUILD + SETTINGS:
        ap.add_argument('--' + key, choices=[v for v, _ in opts], default=opts[d][0],
                        help=f"{label} (default {opts[d][0]})")
    a = ap.parse_args(argv)
    choices = {k: getattr(a, k) for k, _l, _o, _d in BUILD + SETTINGS}
    try:
        run(a.game, a.output, choices, not a.no_folders, not a.no_images, not a.no_harddisk)
    except Exception as e:
        print(f"Error: {e}")
        return 1
    return 0


# ------------------------------------------------------------------ graphical interface
def gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    root = tk.Tk()
    root.title(f"{APP_NAME} {VERSION} - by coffee.crisp")
    frm = ttk.Frame(root, padding=10)
    frm.grid(sticky='nsew')
    root.columnconfigure(0, weight=1); root.rowconfigure(0, weight=1)

    game_var = tk.StringVar()
    out_var = tk.StringVar(value=os.path.join(os.path.expanduser('~'), 'SG8DOS_output'))

    def pick_game():
        p = filedialog.askopenfilename(title="Your STEINS;GATE 8bit data.xp3 or .zip",
                                       filetypes=[("Game data", "*.xp3 *.zip"), ("All files", "*.*")])
        if p: game_var.set(p)

    def pick_game_dir():
        p = filedialog.askdirectory(title="Your STEINS;GATE 8bit folder")
        if p: game_var.set(p)

    def pick_out():
        p = filedialog.askdirectory(title="Output folder")
        if p: out_var.set(p)

    r = 0
    ttk.Label(frm, text="Your game (data.xp3, folder or .zip):").grid(row=r, column=0, sticky='w')
    ttk.Entry(frm, textvariable=game_var, width=50).grid(row=r, column=1, sticky='ew')
    ttk.Button(frm, text="File...", command=pick_game).grid(row=r, column=2)
    ttk.Button(frm, text="Folder...", command=pick_game_dir).grid(row=r, column=3)
    r += 1
    ttk.Label(frm, text="Output folder:").grid(row=r, column=0, sticky='w')
    ttk.Entry(frm, textvariable=out_var, width=50).grid(row=r, column=1, sticky='ew')
    ttk.Button(frm, text="Browse...", command=pick_out).grid(row=r, column=2)
    r += 1

    vars_ = {}

    def combo_frame(title, table, row):
        box = ttk.LabelFrame(frm, text=title, padding=6)
        box.grid(row=row, column=0, columnspan=4, sticky='ew', pady=4)
        for i, (key, label, opts, d) in enumerate(table):
            ttk.Label(box, text=label).grid(row=i // 2, column=(i % 2) * 2, sticky='w', padx=4)
            v = tk.StringVar(value=opts[d][1])
            vars_[key] = (v, opts)
            ttk.Combobox(box, textvariable=v, values=[l for _v, l in opts], state='readonly',
                         width=34).grid(row=i // 2, column=(i % 2) * 2 + 1, sticky='w', padx=4, pady=1)
        return box

    combo_frame("Build", BUILD, r); r += 1
    combo_frame("Settings (SG8.CFG; can be changed later with SETUP on DOS)", SETTINGS, r); r += 1

    out = ttk.LabelFrame(frm, text="Output", padding=6)
    out.grid(row=r, column=0, columnspan=4, sticky='ew')
    folders = tk.BooleanVar(value=True)
    images = tk.BooleanVar(value=True)
    hd = tk.BooleanVar(value=True)
    ttk.Checkbutton(out, text="Floppy folders (DISKn, copy to real floppies)", variable=folders) \
        .grid(row=0, column=0, sticky='w')
    ttk.Checkbutton(out, text="Floppy images (DISKn.IMA, for USB drives, Gotek, emulators)",
                    variable=images).grid(row=0, column=1, sticky='w')
    ttk.Checkbutton(out, text="Ready-to-play folder SG8DOS (hard disk copy, DOSBox)", variable=hd) \
        .grid(row=1, column=0, sticky='w')
    r += 1

    bar = ttk.Progressbar(frm, maximum=100)
    bar.grid(row=r, column=0, columnspan=4, sticky='ew', pady=6)
    r += 1
    logbox = tk.Text(frm, height=12, width=90, state='disabled')
    logbox.grid(row=r, column=0, columnspan=4, sticky='nsew')
    frm.rowconfigure(r, weight=1); frm.columnconfigure(1, weight=1)
    r += 1
    go = ttk.Button(frm, text="Convert")
    go.grid(row=r, column=3, sticky='e', pady=6)
    ttk.Label(frm, text="Fan project. Works only from the game files you give it.") \
        .grid(row=r, column=0, columnspan=3, sticky='w')

    msgs = queue.Queue()

    def log(m): msgs.put(('log', m))

    def progress(m):
        if m.startswith('  [') and '/' in m:
            a, b = m.strip()[1:].split(']')[0].split('/')
            msgs.put(('bar', 100 * int(a) / int(b)))

    def pump():
        try:
            while True:
                kind, v = msgs.get_nowait()
                if kind == 'log':
                    logbox.configure(state='normal'); logbox.insert('end', v + '\n')
                    logbox.see('end'); logbox.configure(state='disabled')
                elif kind == 'bar':
                    bar['value'] = v
                elif kind == 'done':
                    go.state(['!disabled'])
                    if v: messagebox.showinfo(APP_NAME, v)
                elif kind == 'error':
                    go.state(['!disabled'])
                    messagebox.showerror(APP_NAME, v)
        except queue.Empty:
            pass
        root.after(100, pump)

    def start():
        if not game_var.get():
            messagebox.showwarning(APP_NAME, "Choose your game file or folder first.")
            return
        choices = {k: next(val for val, lab in opts if lab == v.get()) for k, (v, opts) in vars_.items()}
        go.state(['disabled']); bar['value'] = 0

        def work():
            try:
                n = run(game_var.get(), out_var.get(), choices, folders.get(),
                        images.get(), hd.get(), log, progress)
                msgs.put(('bar', 100))
                msgs.put(('done', f"Finished: {n} floppy disk(s).\nSee INSTALL.TXT on disk 1."))
            except Exception as e:
                msgs.put(('error', str(e)))
        threading.Thread(target=work, daemon=True).start()

    go.configure(command=start)
    pump()
    root.mainloop()


if __name__ == '__main__':
    problem = check_install()
    if problem:
        print(problem)
        try:
            import tkinter
            from tkinter import messagebox
            tkinter.Tk().withdraw()
            messagebox.showerror(APP_NAME, problem)
        except Exception:
            pass
        sys.exit(1)
    if len(sys.argv) > 1:
        sys.exit(cli(sys.argv[1:]))
    try:
        gui()
    except ImportError:
        print("tkinter is not available: use the command line (python sgso_convert.py --help).")
        print("On Linux, install it with e.g. 'sudo apt install python3-tk'.")
        sys.exit(1)
