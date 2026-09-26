/*
 * SETUP.EXE - settings for SG8.EXE (SG Variant Space Octet DOS). By coffee.crisp.
 * Text-mode menu that detects the hardware and writes SG8.CFG.
 * Build: wcl -bt=dos -ms -0 -ox setup.c -fe=SETUP.EXE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

#define SCR ((u16 far *)MK_FP(0xB800, 0))
#define A_TEXT  0x17
#define A_TITLE 0x1F
#define A_SEL   0x71
#define A_DIM   0x13
#define A_HELP  0x1E

/* ------------------------------------------------------------------ screen */
static void put_str(int x, int y, const char *s, u8 attr)
{
    u16 far *d = SCR + y * 80 + x;
    while (*s && x++ < 80) *d++ = (u16)((attr << 8) | (u8)*s++);
}

static void fill_line(int y, u8 attr)
{
    int x;
    for (x = 0; x < 80; x++) SCR[y * 80 + x] = (u16)((attr << 8) | ' ');
}

static void set_mode(u8 m)
{
    union REGS r;
    r.h.ah = 0; r.h.al = m;
    int86(0x10, &r, &r);
}

static void hide_cursor(void)
{
    union REGS r;
    r.h.ah = 1; r.x.cx = 0x2000;
    int86(0x10, &r, &r);
}

static int getkey(void)
{
    union REGS r;
    r.h.ah = 0;
    int86(0x16, &r, &r);
    if (r.h.al && r.h.al != 0xE0) return r.h.al;
    return r.x.ax & 0xFF00;
}

/* ------------------------------------------------------------------ hardware */
static int display_is_colour(void)
{
    union REGS r;
    r.x.ax = 0x1A00;
    int86(0x10, &r, &r);
    if (r.h.al != 0x1A) return 1;
    return !(r.h.bl == 0x07 || r.h.bl == 0x0B);
}

static void opl_write(u8 reg, u8 val)
{
    int i;
    outp(0x388, reg);
    for (i = 0; i < 6; i++) inp(0x388);
    outp(0x389, val);
    for (i = 0; i < 35; i++) inp(0x388);
}

static int adlib_detect(void)
{
    u8 s1, s2;
    int i;
    opl_write(4, 0x60); opl_write(4, 0x80);
    s1 = (u8)inp(0x388);
    opl_write(2, 0xFF); opl_write(4, 0x21);
    for (i = 0; i < 200; i++) inp(0x388);
    s2 = (u8)inp(0x388);
    opl_write(4, 0x60); opl_write(4, 0x80);
    return (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0;
}

static u16 dos_free_kb(void)
{
    union REGS r;
    r.h.ah = 0x48; r.x.bx = 0xFFFF;
    int86(0x21, &r, &r);
    return (u16)(r.x.bx / 64);
}

static u32 bios_ticks(void) { return *(u32 far *)MK_FP(0x40, 0x6C); }

static void wait_ticks(int n)
{
    u32 t = bios_ticks() + n;
    while (bios_ticks() < t) ;
}

/* ------------------------------------------------------------------ settings */
enum { S_SCREEN, S_SCAN, S_DRAW, S_TEXT, S_TYPING, S_SOUND, S_OCTAVE, S_MUSIC,
       S_VMUSIC, S_VTYPING, S_VEFFECTS, S_COUNT };

typedef struct {
    const char *label;
    const char *key;
    int n;
    const char *choices[5];
    const char *values[5];
    const char *help;
    int cur;
} Setting;

static Setting set[S_COUNT] = {
    { "Screen", "screen", 5,
      { "Automatic", "Colour", "Grey (plasma, LCD)", "Green monitor", "Amber monitor" },
      { "auto", "colour", "grey", "green", "amber" },
      "Automatic uses colour on colour VGA and grey on monochrome/plasma screens.", 0 },
    { "Scanlines", "scanlines", 2, { "On", "Off" }, { "on", "off" },
      "Darkened odd lines, as the original's 200-line PC-8801 display.", 0 },
    { "Picture drawing", "draw", 4,
      { "Instant", "Fast", "PC-8801 (original)", "Slow machines" }, { "0", "1", "2", "3" },
      "How pictures are drawn: strokes, then areas filled one by one.", 2 },
    { "Text speed", "textspeed", 4, { "Instant", "Fast", "Normal (original)", "Slow" },
      { "0", "10", "25", "50" }, "Delay between letters when text is typed out.", 2 },
    { "Typing beep", "typing", 2, { "On", "Off" }, { "on", "off" },
      "Short beep for each letter, as in the original.", 0 },
    { "Sound device", "sound", 3, { "None", "PC speaker / beeper", "AdLib / Sound Blaster" },
      { "none", "speaker", "adlib" },
      "Speaker: one-voice arrangements. AdLib: the three PSG voices of the PC-8801 music.", 1 },
    { "Speaker pitch", "octave", 3, { "Normal", "+1 octave", "+2 octaves" }, { "0", "1", "2" },
      "Raise the pitch on a small piezo beeper, which is quiet on low notes.", 0 },
    { "Music", "music", 2, { "On", "Off" }, { "on", "off" }, "Background music.", 0 },
    { "Music volume", "volmusic", 3, { "Off", "Soft", "Loud" }, { "0", "1", "2" },
      "On a PC speaker, Soft uses very short pulses: it may be very faint.", 2 },
    { "Typing volume", "voltyping", 3, { "Off", "Soft", "Loud" }, { "0", "1", "2" },
      "Volume of the typing beep.", 2 },
    { "Effects volume", "voleffects", 3, { "Off", "Soft", "Loud" }, { "0", "1", "2" },
      "Volume of the sound effects.", 2 },
};

static char extra_lines[10][80];
static int extra_count = 0;

static void load_cfg(void)
{
    FILE *f = fopen("SG8.CFG", "r");
    char line[80], *v;
    int i, k;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char copy[80];
        int known = 0;
        strcpy(copy, line);
        line[strcspn(line, "\r\n")] = 0;
        if (!(v = strchr(line, '='))) continue;
        *v++ = 0;
        for (i = 0; i < S_COUNT; i++)
            if (!stricmp(line, set[i].key)) {
                known = 1;
                for (k = 0; k < set[i].n; k++)
                    if (!stricmp(v, set[i].values[k])) set[i].cur = k;
                if (i == S_SCREEN && !stricmp(v, "grey")) set[i].cur = 2;
            }
        if (!known && extra_count < 10) strcpy(extra_lines[extra_count++], copy);
    }
    fclose(f);
}

static int save_cfg(void)
{
    FILE *f = fopen("SG8.CFG", "w");
    int i;
    if (!f) return 0;
    for (i = 0; i < S_COUNT; i++)
        fprintf(f, "%s=%s\n", set[i].key, set[i].values[set[i].cur]);
    for (i = 0; i < extra_count; i++) fputs(extra_lines[i], f);
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ sound test */
static void spk_tone(u16 hz, int soft)
{
    u16 div = (u16)(1193182UL / hz);
    outp(0x43, soft ? 0xB4 : 0xB6);
    outp(0x42, div & 0xFF); outp(0x42, div >> 8);
    outp(0x61, inp(0x61) | 3);
}

static void spk_off(void) { outp(0x61, inp(0x61) & 0xFC); }

static void opl_setup(void)
{
    int r;
    for (r = 1; r < 0xF6; r++) opl_write((u8)r, 0);
    opl_write(0x01, 0x20);
    opl_write(0x20, 0x21); opl_write(0x40, 0x1A); opl_write(0x60, 0xF0); opl_write(0x80, 0x0F);
    opl_write(0x23, 0x21); opl_write(0x63, 0xF0); opl_write(0x83, 0x0F);
    opl_write(0xC0, 0x0C);
}

static void opl_tone(u16 hz, int soft)
{
    int block;
    u16 fnum = 0;
    for (block = 0; block < 8; block++) {
        fnum = (u16)((u32)hz * (1UL << (20 - block)) / 49716UL);
        if (fnum < 1024) break;
    }
    opl_write(0x43, (u8)(soft ? 16 : 0));
    opl_write(0xA0, (u8)(fnum & 0xFF));
    opl_write(0xB0, (u8)(0x20 | (block << 2) | (fnum >> 8)));
}

static void opl_off(void) { opl_write(0xB0, 0); }

static void sound_test(int adlib_ok)
{
    static const u16 scale[8] = { 262, 294, 330, 349, 392, 440, 494, 523 };
    int dev = set[S_SOUND].cur, i, vol = set[S_VMUSIC].cur;
    int shift = set[S_OCTAVE].cur;
    if (dev == 0 || vol == 0) return;
    if (dev == 2 && !adlib_ok) return;
    if (dev == 2) opl_setup();
    for (i = 0; i < 8; i++) {
        u16 hz = dev == 1 ? (u16)(scale[i] << shift) : scale[i];
        if (dev == 1) spk_tone(hz, vol == 1); else opl_tone(hz, vol == 1);
        wait_ticks(3);
        if (dev == 1) spk_off(); else opl_off();
        wait_ticks(1);
    }
}

/* ------------------------------------------------------------------ menu */
#define ITEM_TEST   S_COUNT
#define ITEM_SAVE   (S_COUNT + 1)
#define ITEM_QUIT   (S_COUNT + 2)
#define ITEMS       (S_COUNT + 3)

static void draw(int sel, int colour, int adlib_ok, u16 kb, const char *msg)
{
    int i, y;
    char buf[80];
    for (y = 0; y < 25; y++) fill_line(y, A_TEXT);
    fill_line(0, A_TITLE);
    put_str(2, 0, "SG Variant Space Octet DOS - Setup            by coffee.crisp", A_TITLE);
    sprintf(buf, "Display: %s VGA    AdLib/Sound Blaster: %s    Free memory: %u KB%s",
            colour ? "colour" : "monochrome", adlib_ok ? "found" : "not found", kb,
            kb < 420 ? " (low!)" : "");
    put_str(2, 2, buf, A_DIM);
    for (i = 0; i < ITEMS; i++) {
        u8 a = i == sel ? A_SEL : A_TEXT;
        y = 4 + i + (i >= S_COUNT ? 1 : 0);
        if (i < S_COUNT) {
            sprintf(buf, " %-18s  < %-24s > ", set[i].label, set[i].choices[set[i].cur]);
        } else if (i == ITEM_TEST) sprintf(buf, " %-48s ", "Test the sound");
        else if (i == ITEM_SAVE) sprintf(buf, " %-48s ", "Save and exit");
        else sprintf(buf, " %-48s ", "Exit without saving");
        put_str(4, y, buf, a);
    }
    put_str(2, 21, sel < S_COUNT ? set[sel].help : "", A_HELP);
    put_str(2, 23, "Up/Down: choose   Left/Right: change   Enter: select   Esc: exit", A_DIM);
    if (msg) put_str(2, 22, msg, A_HELP);
}

int main(void)
{
    int sel = 0, k, colour, adlib_ok;
    u16 kb;
    const char *msg = NULL;

    colour = display_is_colour();
    adlib_ok = adlib_detect();
    kb = dos_free_kb();
    set[S_SOUND].cur = adlib_ok ? 2 : 1;
    load_cfg();

    set_mode(3);
    hide_cursor();
    for (;;) {
        draw(sel, colour, adlib_ok, kb, msg);
        msg = NULL;
        k = getkey();
        if (k == 0x4800) sel = (sel + ITEMS - 1) % ITEMS;
        else if (k == 0x5000) sel = (sel + 1) % ITEMS;
        else if ((k == 0x4B00 || k == 0x4D00) && sel < S_COUNT) {
            int n = set[sel].n;
            set[sel].cur = (set[sel].cur + (k == 0x4D00 ? 1 : n - 1)) % n;
            if (sel == S_SOUND && set[sel].cur == 2 && !adlib_ok)
                msg = "No AdLib / Sound Blaster was detected: SG8 would fall back to the speaker.";
        } else if (k == 13) {
            if (sel == ITEM_TEST) {
                if (set[S_SOUND].cur == 2 && !adlib_ok) msg = "No AdLib / Sound Blaster detected.";
                else if (set[S_SOUND].cur == 0) msg = "Sound device is set to None.";
                else sound_test(adlib_ok);
            } else if (sel == ITEM_SAVE) {
                set_mode(3);
                if (save_cfg()) printf("Settings saved to SG8.CFG. Type SG8 to play.\n");
                else printf("Could not write SG8.CFG.\n");
                return 0;
            } else if (sel == ITEM_QUIT) break;
            else if (sel < S_COUNT) set[sel].cur = (set[sel].cur + 1) % set[sel].n;
        } else if (k == 27) break;
    }
    set_mode(3);
    printf("Settings not changed.\n");
    return 0;
}
