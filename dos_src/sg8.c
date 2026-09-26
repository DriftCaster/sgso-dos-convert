/*
 * SG8.EXE - SG Variant Space Octet DOS, game engine for STEINS;GATE 8bit (English). By coffee.crisp.
 * Reproduces the original game in its recommended PC-8801 mkIISR mode: 640x200 pictures
 * in 8 digital colours with dither tiles, doubled to 400 lines with the darkened
 * scanlines of the original, the game's PC-8801 font, 3-line text window and
 * function key bar. Runs in VGA mode 12h (640x480, 16 colours) on any 8086+.
 * Data: SG8.SCN, SG8.IMG, SG8.VEC, SG8.FNT, SG8.MUS, produced by the convert tool.
 * Build: wcl -bt=dos -ml -0 -ox sg8.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <conio.h>
#include <bios.h>
#include <malloc.h>
#include <math.h>
#include <signal.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef signed short   s16;
typedef unsigned long  u32;

/* ------------------------------------------------------------------ ops */
enum {
    OP_TEXT = 1, OP_DRAW = 3, OP_CLEAR, OP_PUSHDRAW, OP_POPDRAW, OP_WAIT, OP_WAITCLICK,
    OP_INPUT, OP_PROMPT, OP_SET, OP_IF, OP_JMP, OP_GOTO, OP_DONE, OP_SAVEASK, OP_LOADASK,
    OP_SAVE, OP_LOAD, OP_GAMEOVER, OP_RESTART, OP_END, OP_PLAY, OP_STOP, OP_SE, OP_TITLE
};
enum { E_VAR = 1, E_CONST, E_EQ, E_NE, E_AND, E_OR, E_NOT, E_PREFIX };

#define SCR_W       640
#define IMG_H       400
#define TEXT_TOP    400
#define TEXT_LINES  5
#define TEXT_COLS   80
#define COL_TEXT    15
#define COL_DIM     10
#define MAX_INPUT   72

#define SCR_W       640
#define IMG_H       400             /* picture area on screen (200 lines doubled) */
#define PIC_H       200             /* picture resolution, as on the PC-8801 */
#define MAX_INPUT   72

/* Text window: 3 lines of 16 pixels + 2 spacing, 16 px left margin, 8 px right margin,
   as in the original's default text area. Function key bar on the last 16 lines. */
#define TX_TOP      400
#define TX_Y0       402
#define TX_LH       18
#define TX_LINES    3
#define TX_COL0     2
#define TX_COLS     77
#define FK_Y        464

#define C_BLACK     0
#define C_WHITE     7

/* ------------------------------------------------------------------ state */
enum { SND_NONE, SND_SPEAKER, SND_ADLIB };
static int  opt_sound = SND_SPEAKER; /* sound device */
static int  opt_music = 1;
static int  opt_octave = 0;         /* transpose up (piezo beeper) */
static int  opt_speed = 2;          /* drawing: 0 instant, 1 fast, 2 PC-8801, 3 slow machines */
static int  opt_chms = 25;          /* delay per character (ms), 0 = instant */
static int  opt_chse = 1;           /* typing beep per character */
static int  opt_scan = 1;           /* darkened odd lines (the original's scanline blind) */
static int  opt_colour = -1;        /* -1 auto, 0 grey, 1 colour, 2 green, 3 amber monitor */
static int  opt_render_smooth = 0;  /* 16-colour anti-aliased pictures; disables scanline dark-copy mode */
/* Volume per sound type: 0 off, 1 soft, 2 loud. The PC speaker has only two states;
   "soft" replaces the square wave with very short pulses (PIT mode 2). */
enum { VOL_MUSIC, VOL_TYPING, VOL_EFFECTS };
static u8   vol[3] = { 2, 2, 2 };
static double opt_gamma = 1.0;

static u8 far *half_font;           /* 95 glyphs 8x16, ASCII 0x20-0x7E */
static u8 far *zen_font;            /* full-width glyphs 16x16 (2 bytes per row) */
static u16  zen_count;
static u8 far *bios_font8;          /* fallback for other characters */

static FILE *img_fp;
static u16  img_count;
static u32 far *img_off;

static FILE *scn_fp;
static u16  scn_count, scn_sg0, scn_sys, scn_extra;
static u16  nvar[3];
static s16  *vars[3];
static u32  scn_off[128], scn_len[128];
static char scn_name[128][13];

static u8 far *sys_blob;             /* syscmd, always resident */
static u8 far *cur_blob;             /* current scenario */
static int  cur_file = -1;
static u16  pc;

static int  restore_file = -1;
static char f_prompt[64] = "";
static char last_prompt[64] = "command? ";
static char last_input[MAX_INPUT + 1] = "";
static char history[MAX_INPUT + 1] = "";
static int  last_graphic = -1, pushed_graphic = -1;
static int  cur_bgm = -1;

static int  tx_line = 0, tx_col = 0;   /* text cursor in the window */
static int  text_pending = 0;       /* a text line is waiting for a key press */
static int  input_fkeys = 0;        /* show the function key bar during input */

static void quit_game(int code);
static void fatal(const char *msg);
static u16 rd16(FILE *f);
static u32 rd32(FILE *f);

/* ------------------------------------------------------------------ BIOS */
u8 far *bios_font(void);
#pragma aux bios_font = \
    "push bp" \
    "mov ax, 1130h" \
    "mov bh, 6" \
    "int 10h" \
    "mov ax, bp" \
    "pop bp" \
    value [es ax] modify [bx cx dx];

static void set_mode(u8 m)
{
    union REGS r;
    r.h.ah = 0; r.h.al = m;
    int86(0x10, &r, &r);
}

static u8 current_mode(void)
{
    union REGS r;
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    return r.h.al;
}

/* Mode 12h (640x480x16) exists only on VGA. On MDA/CGA/EGA/Hercules -- or an
   emulator set to one of those -- the BIOS leaves the previous mode in place and
   reports no error, so drawing would go to memory that is never displayed. This
   checks the mode actually took and exits with a message instead. */
static void require_vga(void)
{
    if (current_mode() == 0x12) return;
    set_mode(3);
    fputs("This game needs a VGA graphics card (mode 12h, 640x480x16).\n"
          "Your video BIOS did not accept that mode -- if you are using an\n"
          "emulator, check that it is configured for a VGA machine type\n"
          "(not MDA/CGA/EGA/Hercules). A monochrome VGA monitor is fine;\n"
          "use /GREY, /GREEN or /AMBER once the mode itself is working.\n",
          stdout);
    quit_game(1);
}

/* Active display from the VGA BIOS: 1 = colour, 0 = monochrome (plasma, mono VGA). */
static int display_is_colour(void)
{
    union REGS r;
    r.x.ax = 0x1A00;
    int86(0x10, &r, &r);
    if (r.h.al != 0x1A) return 1;
    return !(r.h.bl == 0x07 || r.h.bl == 0x0B);
}

/* Indices 0-7: the PC-8801 digital colours. 8-15: the same at 25% brightness, used on odd
   lines to reproduce the original's scanline blind (black at opacity 192/255). */
static void set_palette(void)
{
    union REGS r; struct SREGS s;
    static u8 dac[48];
    /* monochrome monitor tints: grey (plasma, paper white), green, amber */
    static const double tint[4][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 0.25, 1, 0.35 }, { 1, 0.7, 0.1 } };
    int i, mode = opt_colour >= 0 ? opt_colour : display_is_colour();
    for (i = 0; i < 16; i++) {
        r.x.ax = 0x1000; r.h.bl = (u8)i; r.h.bh = (u8)i;   /* attribute register i -> DAC i */
        int86(0x10, &r, &r);
    }
    r.x.ax = 0x1001; r.h.bh = 0; int86(0x10, &r, &r);     /* black border */
    for (i = 0; i < 16; i++) {
        double rr, gg, bb;
        if (opt_render_smooth && mode == 1) {
            /* Standard 16-colour VGA palette for smooth colour mode. */
            static const u8 vga16[16][3] = {
                {0,0,0},{0,0,42},{0,42,0},{0,42,42},
                {42,0,0},{42,0,42},{42,21,0},{42,42,42},
                {21,21,21},{21,21,63},{21,63,21},{21,63,63},
                {63,21,21},{63,21,63},{63,63,21},{63,63,63}
            };
            rr = vga16[i][0] / 63.0; gg = vga16[i][1] / 63.0; bb = vga16[i][2] / 63.0;
        } else if (opt_render_smooth && mode != 1) {
            /* Sixteen real monochrome levels, tinted for green/amber modes. */
            double y = (double)i / 15.0;
            if (opt_gamma != 1.0 && y > 0) y = pow(y, 1.0 / opt_gamma);
            rr = y * tint[mode][0]; gg = y * tint[mode][1]; bb = y * tint[mode][2];
        } else {
            int c = i & 7;
            double k = (i & 8) ? 63.0 / 255.0 : 1.0;
            rr = (c & 2) ? 1.0 : 0.0; gg = (c & 4) ? 1.0 : 0.0; bb = (c & 1) ? 1.0 : 0.0;
            if (mode != 1) {
                double y = 0.299 * rr + 0.587 * gg + 0.114 * bb;
                if (opt_gamma != 1.0 && y > 0) y = pow(y, 1.0 / opt_gamma);
                rr = y * tint[mode][0]; gg = y * tint[mode][1]; bb = y * tint[mode][2];
            }
            rr *= k; gg *= k; bb *= k;
        }
        dac[i * 3]     = (u8)(rr * 63.0 + 0.5);
        dac[i * 3 + 1] = (u8)(gg * 63.0 + 0.5);
        dac[i * 3 + 2] = (u8)(bb * 63.0 + 0.5);
    }
    segread(&s);
    r.x.ax = 0x1012; r.x.bx = 0; r.x.cx = 16;
    r.x.dx = FP_OFF(dac); s.es = FP_SEG(dac);
    int86x(0x10, &r, &r, &s);
}

static u32 bios_ticks(void)
{
    return *(u32 far *)MK_FP(0x40, 0x6C);
}

/* ------------------------------------------------------------------ VGA */
#define VGA ((u8 far *)MK_FP(0xA000, 0))

static void vga_mask(u8 m) { outp(0x3C4, 2); outp(0x3C5, m); }

/* Colour actually used on screen line y (odd lines darkened). */
static u8 rowc(u8 c, int y)
{
    return (u8)((!opt_render_smooth && opt_scan && (y & 1)) ? (c | 8) : c);
}

static void vga_clear_rows(int y0, int n)
{
    vga_mask(15);
    _fmemset(VGA + (u16)y0 * 80, 0, (u16)n * 80);
}

/* Fills whole bytes: xb..xb+nb-1 (8-pixel columns), rows y..y+h-1, colour c. */
static void fill_bytes(int xb, int y, int nb, int h, u8 c)
{
    int r, p;
    for (r = 0; r < h; r++) {
        u8 cc = rowc(c, y + r);
        u8 far *d = VGA + (u16)(y + r) * 80 + xb;
        for (p = 0; p < 4; p++) {
            vga_mask((u8)(1 << p));
            _fmemset(d, (cc >> p) & 1 ? 0xFF : 0, nb);
        }
    }
    vga_mask(15);
}

/* Draws 8 pixels x 16 rows of glyph bits at byte column xb, screen line y. */
static void put_bits8(int xb, int y, const u8 far *bits, int stride, u8 fg, u8 bg)
{
    int r, p;
    for (r = 0; r < 16; r++) {
        u8 f = rowc(fg, y + r), b = rowc(bg, y + r), g = bits[r * stride];
        u8 far *d = VGA + (u16)(y + r) * 80 + xb;
        for (p = 0; p < 4; p++) {
            vga_mask((u8)(1 << p));
            *d = (u8)(((f >> p) & 1 ? g : 0) | ((b >> p) & 1 ? (u8)~g : 0));
        }
    }
    vga_mask(15);
}

static const u8 far *half_glyph(u8 ch)
{
    if (ch >= 0x20 && ch < 0x7F && half_font) return half_font + (u16)(ch - 0x20) * 16;
    return bios_font8 + (u16)ch * 16;
}

/* Character cell (col) of text line y: half-width char, or a full-width one on 2 cells. */
static void put_char(int col, int y, u8 ch, u8 fg, u8 bg)
{
    put_bits8(col, y, half_glyph(ch), 1, fg, bg);
}

static void put_zen(int col, int y, int idx, u8 fg, u8 bg)
{
    const u8 far *g;
    static const u8 blank[32];
    g = (zen_font && idx < (int)zen_count) ? zen_font + (u16)idx * 32 : (const u8 far *)blank;
    put_bits8(col, y, g, 2, fg, bg);
    if (col + 1 < 80) put_bits8(col + 1, y, g + 1, 2, fg, bg);
}

/* ------------------------------------------------------------------ sound */
/*
 * Music and sound effects on the PC speaker (or a piezo beeper) or on an AdLib /
 * Sound Blaster FM chip (OPL2). PIT channel 0 is sped up to PLAY_HZ; each interrupt
 * advances the tracks, then chains to the previous INT 08h handler at the original
 * rate (18.2 Hz) so the BIOS clock stays correct.
 * Speaker: one-voice PWM arrangements. AdLib: the three PSG channels of the PC-8801
 * arrangement, plus a fourth channel for effects and typing beeps.
 */
#pragma off (check_stack)

#define PLAY_HZ   240
#define PLAY_DIV  ((u16)(1193182UL / PLAY_HZ))
#define MUS_ENTRY 80

typedef struct {
    u16 far *ev;                    /* pairs (value, duration in ticks) */
    u16 n, loop, pos, left;
    u16 val;
    u8  active;
} Voice;

static FILE *mus_fp;
static u8 far *mus_table;           /* per entry: title[48], 4 x (u32 offset, u16 n, u16 loop) */
static u16 mus_count;
static u16 far *trk_buf[3];         /* loaded music tracks */
static u16 far *se_buf[64][2];      /* resident effect tracks: [entry][0 speaker, 1 OPL] */
static volatile Voice v_mus[3], v_sfx;
static u16 blip_ev[2];
static volatile u16 spk_div = 0;
static volatile u8  spk_soft = 0;
static u16 opl_last[4];
static u8  opl_lvl[4];
static u16 tick_acc = 0;
static volatile u32 isr_ticks = 0;
static int timer_hooked;
static void (__interrupt __far *old_int08)(void);
static void (__interrupt __far *old_int23)(void);

static void spk_set(u16 div, u8 soft)
{
    if (div == spk_div && soft == spk_soft) return;
    spk_div = div;
    spk_soft = soft;
    if (!div) {
        outp(0x61, inp(0x61) & 0xFC);
    } else {
        outp(0x43, soft ? 0xB4 : 0xB6);         /* channel 2: mode 2 (pulses) or 3 (square) */
        outp(0x42, div & 0xFF);
        outp(0x42, div >> 8);
        outp(0x61, inp(0x61) | 3);
    }
}

static void opl_write(u8 reg, u8 val)
{
    int i;
    outp(0x388, reg);
    for (i = 0; i < 6; i++) inp(0x388);
    outp(0x389, val);
    for (i = 0; i < 35; i++) inp(0x388);
}

static const u8 opl_car[4] = { 0x03, 0x04, 0x05, 0x0B };

/* Plays OPL word w (F-number | block << 10 | volume << 13) on channel ch at level lv. */
static void opl_note(int ch, u16 w, u8 lv)
{
    u8 tl;
    if (w == opl_last[ch] && lv == opl_lvl[ch]) return;
    if (!w || !lv) {
        opl_write((u8)(0xB0 + ch), (u8)((opl_last[ch] >> 8) & 0x1F));
        opl_last[ch] = 0;
        opl_lvl[ch] = lv;
        return;
    }
    tl = (u8)((7 - (w >> 13)) * 4 + (lv == 1 ? 16 : 0));
    if (tl > 63) tl = 63;
    if (opl_last[ch]) opl_write((u8)(0xB0 + ch), (u8)((opl_last[ch] >> 8) & 0x1F));
    opl_write((u8)(0x40 + opl_car[ch]), tl);
    opl_write((u8)(0xA0 + ch), (u8)(w & 0xFF));
    opl_write((u8)(0xB0 + ch), (u8)(0x20 | ((w >> 8) & 0x1F)));
    opl_last[ch] = w;
    opl_lvl[ch] = lv;
}

static void voice_step(volatile Voice *v)
{
    if (!v->active) return;
    while (v->left == 0) {
        if (v->pos >= v->n) {
            if (v->loop == 0xFFFF || v->loop >= v->n) { v->active = 0; v->val = 0; return; }
            v->pos = v->loop;
        }
        v->val  = v->ev[v->pos * 2];
        v->left = v->ev[v->pos * 2 + 1];
        v->pos++;
    }
    v->left--;
}

static u8 sfx_level(void)
{
    return vol[v_sfx.ev == (u16 far *)blip_ev ? VOL_TYPING : VOL_EFFECTS];
}

static void __interrupt __far int08_handler(void)
{
    int c;
    isr_ticks++;
    voice_step(&v_mus[0]); voice_step(&v_mus[1]); voice_step(&v_mus[2]);
    voice_step(&v_sfx);
    if (opt_sound == SND_SPEAKER) {
        u16 d = 0;
        u8 lv = 0;
        if (v_sfx.active && v_sfx.val) { lv = sfx_level(); d = v_sfx.val; }
        if (!lv && v_mus[0].active) { lv = vol[VOL_MUSIC]; d = v_mus[0].val; }
        if (!lv) d = 0;
        if (d && opt_octave) {
            d >>= opt_octave;
            if (d < 40) d = 40;
        }
        spk_set(d, (u8)(lv == 1));
    } else if (opt_sound == SND_ADLIB) {
        for (c = 0; c < 3; c++)
            opl_note(c, v_mus[c].active ? v_mus[c].val : 0, vol[VOL_MUSIC]);
        opl_note(3, v_sfx.active ? v_sfx.val : 0, sfx_level());
    }
    tick_acc += PLAY_DIV;
    if (tick_acc < PLAY_DIV) _chain_intr(old_int08);
    outp(0x20, 0x20);
}

static void __interrupt __far int23_handler(void)
{
    /* Ctrl+C / Ctrl+Break ignored: the timer must be restored before exiting */
}

#pragma on (check_stack)

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

/* A plain, slightly buzzy voice close to the PSG square waves. */
static void adlib_init(void)
{
    static const u8 mod[4] = { 0x00, 0x01, 0x02, 0x08 };
    int r, c;
    for (r = 1; r < 0xF6; r++) opl_write((u8)r, 0);
    opl_write(0x01, 0x20);
    for (c = 0; c < 4; c++) {
        opl_write((u8)(0x20 + mod[c]), 0x21);
        opl_write((u8)(0x40 + mod[c]), 0x1A);
        opl_write((u8)(0x60 + mod[c]), 0xF0);
        opl_write((u8)(0x80 + mod[c]), 0x0F);
        opl_write((u8)(0xE0 + mod[c]), 0x00);
        opl_write((u8)(0x20 + opl_car[c]), 0x21);
        opl_write((u8)(0x40 + opl_car[c]), 0x3F);
        opl_write((u8)(0x60 + opl_car[c]), 0xF0);
        opl_write((u8)(0x80 + opl_car[c]), 0x0F);
        opl_write((u8)(0xE0 + opl_car[c]), 0x00);
        opl_write((u8)(0xC0 + c), 0x0C);
    }
}

static void adlib_silence(void)
{
    int c;
    for (c = 0; c < 9; c++) opl_write((u8)(0xB0 + c), 0);
}

static void pit0_set(u16 div)
{
    _disable();
    outp(0x43, 0x36);
    outp(0x40, div & 0xFF);
    outp(0x40, div >> 8);
    _enable();
}

static void sound_shutdown(void)
{
    if (!timer_hooked) return;
    timer_hooked = 0;
    pit0_set(0);
    _dos_setvect(0x08, old_int08);
    _dos_setvect(0x23, old_int23);
    outp(0x61, inp(0x61) & 0xFC);
    if (opt_sound == SND_ADLIB) adlib_silence();
}

static u8 far *mus_entry(int id) { return mus_table + (u16)id * MUS_ENTRY; }

static u16 far *load_track(int id, int t, u16 far *buf, u16 *n, u16 *loop)
{
    u8 far *e = mus_entry(id) + 48 + t * 8;
    u32 off = e[0] | ((u32)e[1] << 8) | ((u32)e[2] << 16) | ((u32)e[3] << 24);
    *n = e[4] | (e[5] << 8);
    *loop = e[6] | (e[7] << 8);
    if (!*n) return buf;
    if (!buf && !(buf = (u16 far *)_fmalloc(*n * 4))) { *n = 0; return NULL; }
    fseek(mus_fp, off, SEEK_SET);
    fread(buf, 4, *n, mus_fp);
    return buf;
}

static void load_music(void)
{
    char magic[5] = { 0 };
    int i, t;
    if (opt_sound == SND_NONE) return;
    mus_fp = fopen("SG8.MUS", "rb");
    if (!mus_fp) { opt_sound = SND_NONE; return; }
    fread(magic, 1, 4, mus_fp);
    mus_count = rd16(mus_fp);
    if (strcmp(magic, "SG8N") || !mus_count || mus_count > 64 ||
        !(mus_table = (u8 far *)_fmalloc(mus_count * MUS_ENTRY))) {
        fclose(mus_fp); mus_fp = NULL; opt_sound = SND_NONE; return;
    }
    fread(mus_table, MUS_ENTRY, mus_count, mus_fp);
    for (t = 0; t < 3; t++)
        if (!(trk_buf[t] = (u16 far *)_fmalloc(32000))) { opt_sound = SND_NONE; return; }
    for (i = 0; i < (int)mus_count; i++) {       /* effects: first track does not loop */
        u8 far *e = mus_entry(i) + 48;
        if ((e[6] | (e[7] << 8)) == 0xFFFF) {
            u16 n, loop;
            se_buf[i][0] = load_track(i, 0, NULL, &n, &loop);
            se_buf[i][1] = load_track(i, 1, NULL, &n, &loop);
        }
    }
}

static void sound_init(void)
{
    if (opt_sound == SND_ADLIB && !adlib_detect()) opt_sound = SND_SPEAKER;
    load_music();
    if (opt_sound == SND_ADLIB) adlib_init();
    old_int08 = _dos_getvect(0x08);
    old_int23 = _dos_getvect(0x23);
    _dos_setvect(0x23, int23_handler);
    _dos_setvect(0x08, int08_handler);
    timer_hooked = 1;
    atexit(sound_shutdown);
    pit0_set(PLAY_DIV);
}

static void voice_set(volatile Voice *v, u16 far *ev, u16 n, u16 loop)
{
    _disable();
    v->ev = ev; v->n = n; v->loop = loop;
    v->pos = 0; v->left = 0; v->val = 0;
    v->active = ev && n > 0;
    _enable();
}

static void music_stop(void)
{
    int t;
    cur_bgm = -1;
    for (t = 0; t < 3; t++) { _disable(); v_mus[t].active = 0; _enable(); }
}

static void music_play(int id)
{
    int t;
    u16 n, loop;
    if (id == cur_bgm) return;
    music_stop();
    cur_bgm = id;
    if (!opt_music || opt_sound == SND_NONE || !timer_hooked || id < 0 || id >= (int)mus_count) return;
    if (opt_sound == SND_SPEAKER) {
        load_track(id, 0, trk_buf[0], &n, &loop);
        voice_set(&v_mus[0], trk_buf[0], n, loop == 0xFFFF ? 0 : loop);
    } else {
        for (t = 0; t < 3; t++) {
            load_track(id, t + 1, trk_buf[t], &n, &loop);
            voice_set(&v_mus[t], trk_buf[t], n, loop == 0xFFFF ? 0 : loop);
        }
    }
}

static void sfx_play(int id)
{
    u16 n;
    int t = opt_sound == SND_ADLIB ? 1 : 0;
    u8 far *e;
    if (opt_sound == SND_NONE || !timer_hooked || id < 0 || id >= (int)mus_count || !se_buf[id][t]) return;
    e = mus_entry(id) + 48 + t * 8;
    n = e[4] | (e[5] << 8);
    voice_set(&v_sfx, se_buf[id][t], n, 0xFFFF);
}

/* Typing beep: 0 normal (A4), 1 male (A3), 2 female (A5), ~16 ms as in the original. */
static void text_blip(int voice)
{
    static const u16 divs[3] = { 2712, 5423, 1356 };
    static const u16 opl[3] = { 580 | (4 << 10) | (7 << 13), 580 | (3 << 10) | (7 << 13),
                                580 | (5 << 10) | (7 << 13) };
    if (opt_sound == SND_NONE || !opt_chse || !vol[VOL_TYPING] || !timer_hooked
        || (v_sfx.active && v_sfx.ev != (u16 far *)blip_ev)) return;
    blip_ev[0] = opt_sound == SND_ADLIB ? opl[voice % 3] : divs[voice % 3];
    blip_ev[1] = 4;
    voice_set(&v_sfx, (u16 far *)blip_ev, 1, 0xFFFF);
}

static const char far *music_title(int id)
{
    if (!mus_table || id < 0 || id >= (int)mus_count) return "";
    return (const char far *)mus_entry(id);
}

/* ------------------------------------------------------------------ settings */
/* SG8.CFG: plain text "key=value" lines, written by SETUP.EXE and the volume menu. */
static const char *scr_names[] = { "grey", "colour", "green", "amber" };
static const char *snd_names[] = { "none", "speaker", "adlib" };

static int cfg_pick(const char *v, const char **names, int n)
{
    int i;
    for (i = 0; i < n; i++) if (!stricmp(v, names[i])) return i;
    return -2;
}

static void load_config(void)
{
    FILE *f = fopen("SG8.CFG", "r");
    char line[80], *v;
    int k;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!(v = strchr(line, '='))) continue;
        *v++ = 0;
        if (!stricmp(line, "screen")) opt_colour = (k = cfg_pick(v, scr_names, 4)) >= 0 ? k : -1;
        else if (!stricmp(line, "scanlines")) opt_scan = !stricmp(v, "on");
        else if (!stricmp(line, "rendering")) { opt_render_smooth = !stricmp(v, "smooth"); if (opt_render_smooth) opt_scan = 0; }
        else if (!stricmp(line, "draw")) opt_speed = atoi(v) < 0 ? 0 : atoi(v) > 3 ? 3 : atoi(v);
        else if (!stricmp(line, "textspeed")) opt_chms = atoi(v) < 0 ? 0 : atoi(v);
        else if (!stricmp(line, "typing")) opt_chse = !stricmp(v, "on");
        else if (!stricmp(line, "sound")) { if ((k = cfg_pick(v, snd_names, 3)) >= 0) opt_sound = k; }
        else if (!stricmp(line, "octave")) opt_octave = atoi(v) & 3;
        else if (!stricmp(line, "music")) opt_music = !stricmp(v, "on");
        else if (!stricmp(line, "volmusic")) vol[0] = (u8)(atoi(v) % 3);
        else if (!stricmp(line, "voltyping")) vol[1] = (u8)(atoi(v) % 3);
        else if (!stricmp(line, "voleffects")) vol[2] = (u8)(atoi(v) % 3);
        else if (!stricmp(line, "gamma")) opt_gamma = atof(v);
    }
    fclose(f);
}

/* Rewrites SG8.CFG, keeping the saved settings and updating the volumes. */
static void save_config(void)
{
    static char keep[20][80];
    int n = 0, i;
    FILE *f = fopen("SG8.CFG", "r");
    if (f) {
        while (n < 20 && fgets(keep[n], 80, f)) {
            if (strnicmp(keep[n], "vol", 3)) n++;
        }
        fclose(f);
    }
    if (!(f = fopen("SG8.CFG", "w"))) return;
    for (i = 0; i < n; i++) fputs(keep[i], f);
    fprintf(f, "volmusic=%d\nvoltyping=%d\nvoleffects=%d\n", vol[0], vol[1], vol[2]);
    fclose(f);
}

/* ------------------------------------------------------------------ keyboard */
#define K_ENTER 13
#define K_BS    8
#define K_ESC   27
#define K_UP    0x4800
#define K_F10   0x4400
#define K_F9    0x4300

static int getkey(void)
{
    union REGS r;
    r.h.ah = 0;
    int86(0x16, &r, &r);
    if (r.h.al && r.h.al != 0xE0) return r.h.al;
    return r.x.ax & 0xFF00;
}

static int keyready(void)
{
    return _bios_keybrd(_KEYBRD_READY) != 0;
}

/* Discards keys already waiting in the BIOS buffer. Called before a line is typed
   out so that keys pressed earlier (holding or tapping Enter through a page wait)
   do not immediately cancel the typewriter effect on the following lines. */
static void kbd_flush(void)
{
    while (_bios_keybrd(_KEYBRD_READY)) _bios_keybrd(_KEYBRD_READ);
}

static int shift_down(void)
{
    return (*(u8 far *)MK_FP(0x40, 0x17) & 3) != 0;
}

/* ------------------------------------------------------------------ text console */
/*
 * Text window as in the original (KAG "page" line mode with erafterpage): every text line
 * is typed out letter by letter, then waits for a key with the blinking page-break cursor
 * and the window is cleared. A line longer than the window pages the same way.
 */
typedef struct { u8 zen, v; } Tok;          /* half-width ASCII char, or full-width glyph */

static int fkey_visible = 0;
static int fkey_shift = -1;

static u32 now_ms(void)
{
    u32 t;
    if (!timer_hooked) return bios_ticks() * 10000UL / 182UL;
    _disable(); t = isr_ticks; _enable();
    return t * 25UL / 6UL;              /* 240 Hz -> milliseconds */
}

static int tx_y(void) { return TX_Y0 + tx_line * TX_LH; }

static void window_clear(void)
{
    vga_clear_rows(TX_TOP, FK_Y - TX_TOP);
    tx_line = 0; tx_col = 0;
}

static void con_clear(void)
{
    window_clear();
    text_pending = 0;
}

static void fkey_draw(int shift)
{
    static const char *labels[2][5] = {
        { "left", "back", "front", "right", "phone" },
        { "load ", "save ", "look ", "talk ", "assistant" }
    };
    int i, k;
    vga_clear_rows(FK_Y, 16);
    for (i = 0; i < 5; i++) {
        int xb = (48 + i * 112) / 8;
        fill_bytes(xb - 1, FK_Y, 12, 16, C_WHITE);
        for (k = 0; labels[shift][i][k] && k < 11; k++)
            put_char(xb + k, FK_Y, (u8)labels[shift][i][k], C_BLACK, C_WHITE);
    }
    fkey_visible = 1;
    fkey_shift = shift;
}

static void fkey_hide(void)
{
    if (!fkey_visible) return;
    vga_clear_rows(FK_Y, 16);
    fkey_visible = 0;
}

/* Waits for a key; returns it. Keeps the blinking cursor (glyph at col/y) and the
   function key bar in sync with Shift. cursor: 0 none, 1 page-break triangle, 2 caret. */
static int wait_key_cursor(int cursor, int col, int y, u8 under)
{
    int on = 0, k;
    u32 next = 0;
    for (;;) {
        u32 t = now_ms();
        if (cursor && t >= next) {
            on = !on;
            next = t + 300;
            if (cursor == 1) {
                if (on) put_zen(col, y, 0, C_WHITE, C_BLACK);
                else { put_char(col, y, ' ', C_WHITE, C_BLACK); put_char(col + 1, y, ' ', C_WHITE, C_BLACK); }
            } else {
                if (on) put_char(col, y, under, C_BLACK, C_WHITE);
                else put_char(col, y, under, C_WHITE, C_BLACK);
            }
        }
        if (fkey_visible && shift_down() != fkey_shift) fkey_draw(shift_down());
        if (keyready()) break;
    }
    k = getkey();
    if (cursor == 1) { put_char(col, y, ' ', C_WHITE, C_BLACK); put_char(col + 1, y, ' ', C_WHITE, C_BLACK); }
    else if (cursor == 2) put_char(col, y, under, C_WHITE, C_BLACK);
    return k;
}

static void quit_prompt(void);

/* Page-break wait: blinking triangle after the text, then a key. */
static void page_wait(void)
{
    int col = tx_col <= TX_COLS - 2 ? tx_col : TX_COLS - 2;
    int k = wait_key_cursor(1, TX_COL0 + col, tx_y(), ' ');
    if (k == K_F10) quit_prompt();
}

static void text_ack(void)
{
    if (!text_pending) return;
    text_pending = 0;
    page_wait();
    window_clear();
}

static void tx_newline(void)
{
    if (tx_line < TX_LINES - 1) { tx_line++; tx_col = 0; return; }
    page_wait();
    window_clear();
}

/* Typewriter delay after one character; returns 0 if the player asked to see the whole line. */
static int type_delay(int blip, int voice, u32 *due)
{
    if (blip) text_blip(voice);
    *due += opt_chms;
    while (now_ms() < *due) {
        if (keyready()) {
            if (getkey() == K_F10) quit_prompt();
            return 0;
        }
    }
    return 1;
}

static void put_tok(Tok t)
{
    if (t.zen) put_zen(TX_COL0 + tx_col, tx_y(), t.v, C_WHITE, C_BLACK);
    else put_char(TX_COL0 + tx_col, tx_y(), t.v, C_WHITE, C_BLACK);
    tx_col += t.zen ? 2 : 1;
}

/* Prints tokens with word wrap, starting where the cursor is. */
static void print_toks(const Tok *t, int n, int typed, int voice)
{
    int i = 0;
    u32 due = 0;
    if (typed && (!opt_chms || !timer_hooked)) typed = 0;
    if (typed) { kbd_flush(); due = now_ms(); }
    while (i < n) {
        int j = i, w = 0;
        while (j < n && !(!t[j].zen && t[j].v == ' ')) { w += t[j].zen ? 2 : 1; j++; }
        if (tx_col > 0 && tx_col + w > TX_COLS) tx_newline();
        while (i < j) {
            int cw = t[i].zen ? 2 : 1;
            if (tx_col + cw > TX_COLS) tx_newline();
            put_tok(t[i]);
            if (typed && !type_delay(1, voice, &due)) typed = 0;
            i++;
        }
        while (i < n && !t[i].zen && t[i].v == ' ') {
            if (tx_col < TX_COLS) {
                put_tok(t[i]);
                if (typed && !type_delay(0, voice, &due)) typed = 0;
            }
            i++;
        }
    }
}

static int str_toks(const char *s, Tok *t, int max)
{
    int n = 0;
    while (*s && n < max) { t[n].zen = 0; t[n].v = (u8)*s++; n++; }
    return n;
}

/* One line of text as the original shows it: waits for the previous line first. */
static void text_line(const Tok *t, int n, int typed, int voice)
{
    text_ack();
    if (tx_col > 0) tx_newline();
    print_toks(t, n, typed, voice);
    text_pending = 1;
}

static void con_print(const char *s)
{
    static Tok t[1024];
    int n = str_toks(s, t, 160);
    text_line(t, n, 1, 0);
}

/* A line of the DOS-only menus: shown at once, without waiting. */
static void con_status(const char *s)
{
    static Tok t[1024];
    int n = str_toks(s, t, 160);
    if (tx_col > 0) tx_newline();
    print_toks(t, n, 0, 0);
}

static void volume_menu(void);
static int  in_volume_menu = 0;

static void draw_input(int x0, const char *buf, int len, int clear_to)
{
    int i;
    for (i = 0; i < len; i++) put_char(x0 + i, tx_y(), (u8)buf[i], C_WHITE, C_BLACK);
    for (; i < clear_to; i++) put_char(x0 + i, tx_y(), ' ', C_WHITE, C_BLACK);
}

/* Reads a line after the prompt. maxlen = 0: as long as the window allows. */
static void con_input(const char *prompt, int maxlen, char *out)
{
    int len = 0, k, i, lim, x0, plen = (int)strlen(prompt);
    char buf[MAX_INPUT + 1];

    text_ack();
    if (tx_col > 0) tx_newline();
    for (i = 0; prompt[i]; i++) put_char(TX_COL0 + tx_col++, tx_y(), (u8)prompt[i], C_WHITE, C_BLACK);
    x0 = TX_COL0 + tx_col;
    lim = TX_COLS - 1 - plen;
    if (lim > MAX_INPUT) lim = MAX_INPUT;
    if (maxlen > 0 && maxlen < lim) lim = maxlen;
    if (input_fkeys) fkey_draw(shift_down());
    for (;;) {
        k = wait_key_cursor(2, x0 + len, tx_y(), ' ');
        if (k == K_ENTER) break;
        if (k == K_F10) { quit_prompt(); continue; }
        if (k == K_F9 && !in_volume_menu) {
            int fk = fkey_visible;
            volume_menu();
            if (tx_col > 0) tx_newline();
            tx_col = 0;
            for (i = 0; prompt[i]; i++) put_char(TX_COL0 + tx_col++, tx_y(), (u8)prompt[i], C_WHITE, C_BLACK);
            x0 = TX_COL0 + tx_col;
            draw_input(x0, buf, len, len);
            if (fk) fkey_draw(shift_down());
            continue;
        }
        if (k == K_BS) {
            if (len > 0) { len--; put_char(x0 + len, tx_y(), ' ', C_WHITE, C_BLACK); }
            continue;
        }
        if (k == K_ESC || k == K_UP) {
            int old = len;
            len = 0;
            if (k == K_UP)
                for (i = 0; history[i] && len < lim; i++) buf[len++] = history[i];
            draw_input(x0, buf, len, old > len ? old : len);
            continue;
        }
        if (fkey_visible && ((k >= 0x3B00 && k <= 0x3F00) || (k >= 0x5400 && k <= 0x5800))) {
            static const char *fk[2][5] = {
                { "left", "back", "front", "right", "phone" },
                { "load ", "save ", "look ", "talk ", "assistant" }
            };
            const char *w = k >= 0x5400 ? fk[1][(k - 0x5400) >> 8] : fk[0][(k - 0x3B00) >> 8];
            for (i = 0; w[i] && len < lim; i++) {
                buf[len] = w[i];
                put_char(x0 + len++, tx_y(), (u8)w[i], C_WHITE, C_BLACK);
            }
            continue;
        }
        if (k >= 32 && k < 127 && len < lim) {
            buf[len] = (char)k;
            put_char(x0 + len++, tx_y(), (u8)k, C_WHITE, C_BLACK);
        }
    }
    buf[len] = 0;
    strcpy(out, buf);
    if (len) strcpy(history, buf);
    fkey_hide();
    tx_col = TX_COLS;                   /* the next output starts on a new line */
}

static void wait_click(void)
{
    text_pending = 0;
    page_wait();
}

static void wait_ms(u16 ms)
{
    u32 t = now_ms() + ms;
    while (now_ms() < t) {
        if (keyready()) {
            if (getkey() == K_F10) quit_prompt();
            break;
        }
    }
}

/* ------------------------------------------------------------------ images */
/*
 * Pictures are 640x200 in the 8 digital colours (3 bit planes), shown with every line
 * doubled: the second line of each pair uses the darkened copy of the colour (scanlines).
 * With SG8.VEC they are drawn again like in the original, element by element: white
 * canvas, strokes, then fills revealed row by row from the final picture.
 */
#define PLANE_SIZE ((u16)PIC_H * 80)
#define MAX_EDGES  2000

static u8 far *plane[4];
static FILE *vec_fp;
static u16  vec_count;
static u32 far *vec_off;

typedef struct { s16 x0, y0, x1, y1; } Edge;
static Edge far *edges;
static s16 far *pts_x, far *pts_y;
static int  anim_skip;
static u32  anim_due;

static void clear_image(void)
{
    vga_clear_rows(0, IMG_H);
}

/* Writes picture row y (0-199) to its two screen lines. */
static void blit_row(int y)
{
    u8 far *d0 = VGA + (u16)(2 * y) * 80;
    u8 far *d1 = d0 + 80;
    int p;
    for (p = 0; p < 4; p++) {
        vga_mask((u8)(1 << p));
        _fmemcpy(d0, plane[p] + (u16)y * 80, 80);
        _fmemcpy(d1, plane[p] + (u16)y * 80, 80);
    }
    if (!opt_render_smooth) {
        vga_mask(8);
        _fmemset(d0, 0, 80);
        _fmemset(d1, opt_scan ? 0xFF : 0, 80);
    }
    vga_mask(15);
}

static void blit_image(void)
{
    int y;
    for (y = 0; y < PIC_H; y++) blit_row(y);
}

static int decode_image(int id)
{
    static u8 row[80];
    int y, p, n, c;
    u32 off;
    if (id < 0 || id >= (int)img_count || !(off = img_off[id])) return 0;
    fseek(img_fp, off, SEEK_SET);
    for (y = 0; y < PIC_H; y++) {
        for (p = 0; p < 4; p++) {
            n = 0;
            while (n < 80) {
                c = getc(img_fp);
                if (c == EOF) { memset(row + n, 0, 80 - n); break; }
                if (c < 128) {
                    c++;
                    while (c-- && n < 80) row[n++] = (u8)getc(img_fp);
                } else if (c > 128) {
                    int v = getc(img_fp);
                    c = 257 - c;
                    while (c-- && n < 80) row[n++] = (u8)v;
                }
            }
            _fmemcpy(plane[p] + (u16)y * 80, row, 80);
        }
    }
    return 1;
}

/* Waits until the drawing deadline; returns 1 if the player asked to skip. */
static int anim_wait(void)
{
    if (anim_skip) return 1;
    do {
        if (keyready()) {
            int k = getkey();
            if (k == K_F10) quit_prompt();
            anim_skip = 1;
            return 1;
        }
    } while (now_ms() < anim_due);
    return 0;
}

static void anim_sync(void)
{
    u32 now = now_ms();
    if (anim_due < now) anim_due = now;
}

/* One picture pixel (x, y in 640x200) in colour c, as two screen pixels (both lines). */
static void put_pixel(int x, int y, u8 c)
{
    u16 off;
    volatile u8 latch;
    if ((unsigned)x >= SCR_W || (unsigned)y >= PIC_H) return;
    off = (u16)(2 * y) * 80 + (x >> 3);
    outp(0x3CE, 8); outp(0x3CF, (u8)(0x80 >> (x & 7)));
    outp(0x3CE, 0); outp(0x3CF, c);
    latch = VGA[off]; VGA[off] = latch;
    outp(0x3CE, 0); outp(0x3CF, rowc(c, 1));
    latch = VGA[off + 80]; VGA[off + 80] = latch;
}

/* Stroke as drawn by the original: 1-pixel line, drawn twice (x and x+1). */
static void draw_line(int x0, int y0, int x1, int y1, u8 c)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, e2;
    for (;;) {
        put_pixel(x0, y0, c);
        put_pixel(x0 + 1, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void line_mode_on(void)
{
    vga_mask(15);
    outp(0x3CE, 1); outp(0x3CF, 0x0F);          /* set/reset on all planes */
}

static void line_mode_off(void)
{
    outp(0x3CE, 1); outp(0x3CF, 0);
    outp(0x3CE, 0); outp(0x3CF, 0);
    outp(0x3CE, 8); outp(0x3CF, 0xFF);
}

/* Copies pixels [xa, xb] of picture row y from the final picture (both screen lines). */
static void copy_span(int y, int xa, int xb)
{
    u16 b0, b1, b, line;
    u8 m;
    int p, half;
    volatile u8 latch;
    if (xa < 0) xa = 0;
    if (xb > SCR_W - 1) xb = SCR_W - 1;
    if (xa > xb || (unsigned)y >= PIC_H) return;
    b0 = xa >> 3; b1 = xb >> 3;
    for (half = 0; half < 2; half++) {
        line = (u16)(2 * y + half) * 80;
        for (b = b0; b <= b1; b++) {
            m = 0xFF;
            if (b == b0) m &= (u8)(0xFF >> (xa & 7));
            if (b == b1) m &= (u8)(0xFF << (7 - (xb & 7)));
            outp(0x3CE, 8); outp(0x3CF, m);
            latch = VGA[line + b];
            for (p = 0; p < 4; p++) { vga_mask((u8)(1 << p)); VGA[line + b] = plane[p][(u16)y * 80 + b]; }
            if (!opt_render_smooth) { vga_mask(8); VGA[line + b] = (u8)((half && opt_scan) ? 0xFF : 0); }
        }
    }
    (void)latch;
    vga_mask(15);
    outp(0x3CE, 8); outp(0x3CF, 0xFF);
}

static s16 vrd16(void) { u16 v = (u16)getc(vec_fp); return (s16)(v | ((u16)getc(vec_fp) << 8)); }

/* Reads a polyline into pts_x/pts_y; returns the number of points kept. */
static int read_poly(void)
{
    int n = (u16)vrd16(), i, kept = 0;
    s16 x = vrd16(), y = vrd16();
    for (i = 0; i < n; i++) {
        if (i > 0) {
            int dx = (signed char)getc(vec_fp);
            if (dx == -128) { x = vrd16(); y = vrd16(); }
            else { x += dx; y += (signed char)getc(vec_fp); }
        }
        if (kept < MAX_EDGES) { pts_x[kept] = x; pts_y[kept] = y; kept++; }
    }
    return kept;
}

static void bbox_add(int n, int *bx0, int *by0, int *bx1, int *by1)
{
    int k;
    for (k = 0; k < n; k++) {
        if (pts_x[k] < *bx0) *bx0 = pts_x[k];
        if (pts_x[k] > *bx1) *bx1 = pts_x[k];
        if (pts_y[k] < *by0) *by0 = pts_y[k];
        if (pts_y[k] > *by1) *by1 = pts_y[k];
    }
}

static long clip_area(int bx0, int by0, int bx1, int by1)
{
    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 > SCR_W) bx1 = SCR_W;
    if (by1 > PIC_H) by1 = PIC_H;
    if (bx1 <= bx0 || by1 <= by0) return 0;
    return (long)(bx1 - bx0) * (by1 - by0);
}

/* Stroke element. Waits as the original: none on the PC-8801 (SpeedCoef 0.005),
   area/50 ms clamped to 1-25 on the slower machines (/V:3). */
static void anim_lines(u8 color)
{
    int np = (u16)vrd16(), i, k, n;
    int bx0 = 32767, by0 = 32767, bx1 = -32768, by1 = -32768;
    line_mode_on();
    for (i = 0; i < np; i++) {
        n = read_poly();
        if (anim_skip) continue;
        bbox_add(n, &bx0, &by0, &bx1, &by1);
        if (n == 1) { draw_line(pts_x[0], pts_y[0], pts_x[0], pts_y[0], color); continue; }
        for (k = 1; k < n; k++) draw_line(pts_x[k - 1], pts_y[k - 1], pts_x[k], pts_y[k], color);
    }
    line_mode_off();
    if (!anim_skip && opt_speed == 3) {
        long a = clip_area(bx0, by0, bx1, by1) / 50;
        anim_sync();
        anim_due += a < 1 ? 1 : a > 25 ? 25 : a;
        anim_wait();
    }
}

/* Fill element: rows of the polygon revealed from the final picture. The original shows
   one fill per frame on the PC-8801 (about 16 ms); slower machines take area-based time. */
static void anim_fill(u8 stroke)
{
    int np = (u16)vrd16(), i, k, n, ne = 0, y, ymin = 32767, ymax = -32768, rows;
    int bx0 = 32767, bx1 = -32768;
    static s16 xs[64];
    u32 start, dur;
    for (i = 0; i < np; i++) {
        n = read_poly();
        for (k = 0; k < n; k++) {
            if (pts_x[k] < bx0) bx0 = pts_x[k];
            if (pts_x[k] > bx1) bx1 = pts_x[k];
        }
        for (k = 0; k < n && ne < MAX_EDGES; k++) {
            int j = (k + 1) % n;
            Edge far *e;
            if (pts_y[k] == pts_y[j]) continue;
            e = &edges[ne++];
            if (pts_y[k] < pts_y[j]) { e->x0 = pts_x[k]; e->y0 = pts_y[k]; e->x1 = pts_x[j]; e->y1 = pts_y[j]; }
            else { e->x0 = pts_x[j]; e->y0 = pts_y[j]; e->x1 = pts_x[k]; e->y1 = pts_y[k]; }
            if (e->y0 < ymin) ymin = e->y0;
            if (e->y1 > ymax) ymax = e->y1;
        }
    }
    (void)stroke;
    if (anim_skip || !ne) return;
    if (opt_speed == 1) dur = 4;
    else if (opt_speed == 3) {
        long a = clip_area(bx0, ymin, bx1, ymax) * 2 / 100;
        dur = a < 16 ? 16 : a > 100 ? 100 : (u32)a;
    } else dur = 16;
    if (ymin < 0) ymin = 0;
    if (ymax > PIC_H) ymax = PIC_H;
    rows = ymax - ymin;
    anim_sync();
    start = anim_due;
    if (rows > 0) {
        for (y = ymin; y < ymax; y++) {
            int nx = 0;
            for (k = 0; k < ne; k++) {
                Edge far *e = &edges[k];
                /* crossing at the pixel centre line; GDI+ samples at integer coordinates */
                if (y >= e->y0 && y < e->y1 && nx < 64) {
                    long num = (long)(y - e->y0) * (e->x1 - e->x0);
                    long den = (long)(e->y1 - e->y0);
                    int x = e->x0 + (int)((num >= 0 ? num + den / 2 : num - den / 2) / den);
                    int m = nx++;
                    while (m > 0 && xs[m - 1] > x) { xs[m] = xs[m - 1]; m--; }
                    xs[m] = (s16)x;
                }
            }
            for (k = 0; k + 1 < nx; k += 2)
                if (xs[k + 1] >= xs[k]) copy_span(y, xs[k], xs[k + 1]);
            anim_due = start + dur * (u32)(y - ymin + 1) / rows;
            if (dur > 16 && anim_wait()) return;
        }
    }
    anim_due = start + dur;
    anim_wait();
}

static void animate_image(int id)
{
    int tag;
    anim_skip = 0;
    anim_due = now_ms();
    fseek(vec_fp, vec_off[id], SEEK_SET);
    fill_bytes(0, 0, 80, IMG_H, C_WHITE);       /* white canvas */
    while ((tag = getc(vec_fp)) > 0 && tag != EOF) {
        u8 col = (u8)getc(vec_fp);
        if (tag == 1) anim_lines(col);
        else if (tag == 2) anim_fill(col);
        else break;
        if (anim_skip) break;
    }
}

/* animate: 1 for progressive drawing (draw/redraw), 0 for direct display. */
static void draw_image(int id, int animate)
{
    if (!decode_image(id)) {
        clear_image();
        return;
    }

    /*
     * Smooth pictures are already anti-aliased final frames. Replaying the
     * authentic vector strokes over them can temporarily put a stroke across
     * a character/object before a later fill covers it. Keep vector replay
     * for Authentic mode, but show the completed Smooth frame directly.
     */
    if (animate && opt_render_smooth) {
        blit_image();
        return;
    }

    if (animate && opt_speed && vec_fp && timer_hooked && id < (int)vec_count && vec_off[id])
        animate_image(id);
    blit_image();
}

static void show_graphic(int id)
{
    last_graphic = id;
    draw_image(id, 0);
}

static void paint_graphic(int id)
{
    last_graphic = id;
    draw_image(id, 1);
}

static void open_vec(void)
{
    char magic[5] = { 0 };
    u16 i;
    int p;
    for (p = 0; p < 4; p++)
        if (!(plane[p] = (u8 far *)_fmalloc(PLANE_SIZE))) fatal("not enough memory (image)");
    vec_fp = fopen("SG8.VEC", "rb");
    if (!vec_fp) return;
    setvbuf(vec_fp, NULL, _IOFBF, 4096);
    fread(magic, 1, 4, vec_fp);
    edges = (Edge far *)_fmalloc(MAX_EDGES * sizeof(Edge));
    pts_x = (s16 far *)_fmalloc(MAX_EDGES * sizeof(s16));
    pts_y = (s16 far *)_fmalloc(MAX_EDGES * sizeof(s16));
    if (strcmp(magic, "SG8B") || !edges || !pts_x || !pts_y) { fclose(vec_fp); vec_fp = NULL; return; }
    vec_count = rd16(vec_fp);
    vec_off = (u32 far *)_fmalloc(vec_count * sizeof(u32));
    if (!vec_off) { fclose(vec_fp); vec_fp = NULL; return; }
    for (i = 0; i < vec_count; i++) vec_off[i] = rd32(vec_fp);
}

/* ------------------------------------------------------------------ files */
static void fatal(const char *msg)
{
    set_mode(3);
    printf("SG8: %s\n", msg);
    exit(1);
}

static u16 rd16(FILE *f) { u16 v = (u16)getc(f); return v | ((u16)getc(f) << 8); }
static u32 rd32(FILE *f) { u32 v = rd16(f); return v | ((u32)rd16(f) << 16); }

static void open_data(void)
{
    char magic[5] = { 0 };
    u16 i;
    int k;

    img_fp = fopen("SG8.IMG", "rb");
    scn_fp = fopen("SG8.SCN", "rb");
    if (!img_fp || !scn_fp) fatal("SG8.IMG / SG8.SCN not found (run SG8 from the game folder).");
    setvbuf(img_fp, NULL, _IOFBF, 8192);

    fread(magic, 1, 4, img_fp);
    if (strcmp(magic, "SG8J")) fatal("SG8.IMG is invalid or from an older version");
    img_count = rd16(img_fp);
    img_off = (u32 far *)_fmalloc(img_count * sizeof(u32));
    if (!img_off) fatal("not enough memory");
    for (i = 0; i < img_count; i++) { img_off[i] = rd32(img_fp); rd32(img_fp); }

    fread(magic, 1, 4, scn_fp);
    if (strcmp(magic, "SG8S")) fatal("SG8.SCN is invalid");
    scn_count = rd16(scn_fp); scn_sg0 = rd16(scn_fp);
    scn_sys = rd16(scn_fp); scn_extra = rd16(scn_fp);
    if (scn_count > 128) fatal("SG8.SCN: too many scenarios");
    for (k = 0; k < 3; k++) {
        nvar[k] = rd16(scn_fp);
        vars[k] = (s16 *)calloc(nvar[k] + 1, sizeof(s16));
        if (!vars[k]) fatal("not enough memory");
    }
    for (i = 0; i < scn_count; i++) {
        fread(scn_name[i], 1, 12, scn_fp); scn_name[i][12] = 0;
        scn_off[i] = rd32(scn_fp); scn_len[i] = rd32(scn_fp);
    }
}

/* SG8.FNT: the game's PC-8801 font for ASCII, then the full-width glyphs. */
static void load_font(void)
{
    FILE *f = fopen("SG8.FNT", "rb");
    char magic[5] = { 0 };
    bios_font8 = bios_font();
    if (!f) return;
    fread(magic, 1, 4, f);
    if (!strcmp(magic, "SG8F") && (half_font = (u8 far *)_fmalloc(95 * 16)) != NULL) {
        fread(half_font, 1, 95 * 16, f);
        zen_count = rd16(f);
        if (zen_count && (zen_font = (u8 far *)_fmalloc(zen_count * 32)) != NULL)
            fread(zen_font, 1, zen_count * 32, f);
        else zen_count = 0;
    }
    fclose(f);
}

static u8 far *load_blob(int id)
{
    u8 far *b = (u8 far *)_fmalloc((u16)scn_len[id]);
    if (!b) fatal("not enough memory (scenario)");
    fseek(scn_fp, scn_off[id], SEEK_SET);
    if (fread(b, 1, (u16)scn_len[id], scn_fp) != (u16)scn_len[id]) fatal("cannot read SG8.SCN");
    return b;
}

static void switch_file(int id)
{
    if (id == cur_file) return;
    if (cur_blob && cur_blob != sys_blob) _ffree(cur_blob);
    cur_blob = (id == (int)scn_sys) ? sys_blob : load_blob(id);
    cur_file = id;
}

static u16 code_start(u8 far *b) { return b[0] | (b[1] << 8); }

/* Finds a label; returns its absolute offset in the blob, or 0. */
static u16 find_label(u8 far *b, const char *name)
{
    u16 n = b[2] | (b[3] << 8), i, p = 4, len = (u16)strlen(name);
    for (i = 0; i < n; i++) {
        u16 off = b[p] | (b[p + 1] << 8);
        u8 l = b[p + 2];
        if (l == len && !_fmemcmp(b + p + 3, name, l)) return code_start(b) + off;
        p += 3 + l;
    }
    return 0;
}

/* ------------------------------------------------------------------ saves */
static void load_sysflags(void)
{
    FILE *f = fopen("SG8SYS.DAT", "rb");
    u16 n, i;
    if (!f) return;
    n = rd16(f);
    for (i = 0; i < n && i < nvar[2]; i++) vars[2][i] = (s16)rd16(f);
    fclose(f);
}

static void save_sysflags(void)
{
    FILE *f = fopen("SG8SYS.DAT", "wb");
    if (!f) return;
    fwrite(&nvar[2], 2, 1, f);
    fwrite(vars[2], 2, nvar[2], f);
    fclose(f);
}

static void slot_name(int n, char *buf) { sprintf(buf, "SG8SAV%d.DAT", n); }

static int slot_date(int n, char *date)
{
    char name[16], magic[5] = { 0 };
    FILE *f;
    slot_name(n, name);
    if (!(f = fopen(name, "rb"))) return 0;
    fread(magic, 1, 4, f);
    fread(date, 1, 20, f);
    date[19] = 0;
    fclose(f);
    return !strcmp(magic, "SG8V") || !strcmp(magic, "SG8W");
}

static int write_slot(int n)
{
    char name[16], date[20];
    struct dosdate_t d; struct dostime_t t;
    FILE *f;
    s16 v;
    slot_name(n, name);
    if (!(f = fopen(name, "wb"))) return 0;
    _dos_getdate(&d); _dos_gettime(&t);
    memset(date, 0, sizeof date);
    sprintf(date, "%04d/%02d/%02d %02d:%02d", d.year, d.month, d.day, t.hour, t.minute);
    fwrite("SG8W", 1, 4, f);
    fwrite(date, 1, 20, f);
    v = (s16)(restore_file >= 0 ? restore_file : cur_file); fwrite(&v, 2, 1, f);
    v = (s16)last_graphic; fwrite(&v, 2, 1, f);
    v = (s16)pushed_graphic; fwrite(&v, 2, 1, f);
    v = (s16)cur_bgm; fwrite(&v, 2, 1, f);
    fwrite(last_prompt, 1, 64, f);
    fwrite(&nvar[0], 2, 1, f);
    fwrite(vars[0], 2, nvar[0], f);
    fclose(f);
    return 1;
}

static int read_slot(int n)
{
    char name[16], magic[5] = { 0 }, date[20];
    FILE *f;
    s16 file, g, pg, bgm = -1;
    u16 nf, i;
    slot_name(n, name);
    if (!(f = fopen(name, "rb"))) return 0;
    fread(magic, 1, 4, f);
    fread(date, 1, 20, f);
    file = (s16)rd16(f); g = (s16)rd16(f); pg = (s16)rd16(f);
    if (!strcmp(magic, "SG8W")) bgm = (s16)rd16(f);
    fread(last_prompt, 1, 64, f); last_prompt[63] = 0;
    nf = rd16(f);
    memset(vars[0], 0, nvar[0] * 2);
    memset(vars[1], 0, nvar[1] * 2);
    for (i = 0; i < nf; i++) {
        s16 v = (s16)rd16(f);
        if (i < nvar[0]) vars[0][i] = v;
    }
    fclose(f);
    if ((strcmp(magic, "SG8V") && strcmp(magic, "SG8W")) || file < 0 || file >= (s16)scn_count) return 0;
    switch_file(file);
    restore_file = -1;
    pushed_graphic = pg;
    music_stop();
    if (bgm >= 0) music_play(bgm);
    show_graphic(g);
    return 1;
}

/* ------------------------------------------------------------------ VM */
static void do_input(const char *prompt);
static void restart(void);

static void do_done(void)
{
    if (restore_file < 0) return;
    switch_file(restore_file);
    restore_file = -1;
    do_input(last_prompt);
}

static int ask_yn(const char *prompt, int dflt)
{
    char a[4];
    con_input(prompt, 1, a);
    if (!a[0]) return dflt;
    return a[0] == 'y' || a[0] == 'Y';
}

static void do_save(int n)
{
    char date[20], msg[80];
    if (slot_date(n, date)) {
        sprintf(msg, "Slot %d has data from [%s].", n, date);
        con_print(msg);
        if (!ask_yn("Overwrite (y/N)? ", 0)) { con_print("Canceled."); do_done(); return; }
    }
    if (write_slot(n)) sprintf(msg, "Saved to slot %d.", n);
    else sprintf(msg, "Could not write save file.");
    con_print(msg);
    do_done();
}

static void do_load(int n)
{
    char date[20], msg[80];
    if (!slot_date(n, date)) {
        sprintf(msg, "Slot %d has no save data.", n);
        con_print(msg);
        do_done();
        return;
    }
    sprintf(msg, "Slot %d has data from [%s].", n, date);
    con_print(msg);
    if (!ask_yn("Load this data (Y/n)? ", 1)) { con_print("Canceled."); do_done(); return; }
    con_clear();
    if (!read_slot(n)) { con_print("Save data is damaged."); do_done(); return; }
    do_input(last_prompt);
}

static void do_slot_ask(int save)
{
    char a[4];
    con_input(save ? "Save to which slot(0-9)? " : "Load from which slot(0-9)? ", 1, a);
    if (a[0] >= '0' && a[0] <= '9') {
        if (save) do_save(a[0] - '0'); else do_load(a[0] - '0');
    } else {
        con_print("Canceled.");
        do_done();
    }
}

static void normalize(const char *in, char *out)
{
    int sp = 0, n = 0;
    while (*in == ' ') in++;
    for (; *in; in++) {
        if (*in == ' ') { sp = 1; continue; }
        if (sp && n) out[n++] = ' ';
        sp = 0;
        out[n++] = (char)tolower((u8)*in);
    }
    out[n] = 0;
}

static const char *vol_name(int v) { return v == 2 ? "loud" : v == 1 ? "soft" : "off"; }

static void volume_menu(void)
{
    char a[4], line[80];
    in_volume_menu = 1;
    for (;;) {
        window_clear();
        sprintf(line, "Volume - Music: %s   Typing: %s   Effects: %s",
                vol_name(vol[0]), vol_name(vol[1]), vol_name(vol[2]));
        con_status(line);
        if (opt_sound == SND_NONE) con_status("(Sound is off: run SETUP to choose a sound device.)");
        con_input("Change m/t/e (Enter = done)? ", 1, a);
        a[0] = (char)tolower((u8)a[0]);
        if (a[0] == 'm' || a[0] == 't' || a[0] == 'e') {
            int i = a[0] == 'm' ? 0 : a[0] == 't' ? 1 : 2;
            vol[i] = (u8)(vol[i] ? vol[i] - 1 : 2);
            if (i == VOL_TYPING) text_blip(0);
            continue;
        }
        break;
    }
    save_config();
    window_clear();
    in_volume_menu = 0;
}

/* Reads a command and jumps to the matching label (same logic as the original). */
static void do_input(const char *prompt)
{
    char raw[MAX_INPUT + 1], key[MAX_INPUT + 1];
    u16 off;
    int special = (cur_file == (int)scn_sys || cur_file == (int)scn_extra);
    /* the title and music gallery prompts have no function key bar (nofunc) */
    int fkeys = strcmp(prompt, "start/load/help? ") && strcmp(prompt, "select(1-18)? ");
    strcpy(last_prompt, prompt);
    for (;;) {
        input_fkeys = fkeys;
        con_input(prompt, 0, raw);
        input_fkeys = 0;
        normalize(raw, key);
        if (!key[0]) { continue; }
        strcpy(last_input, raw);
        { char *a = last_input, *b = last_input; while (*a == ' ') a++; while ((*b++ = *a++) != 0) ; }
        if ((off = find_label(cur_blob, key)) != 0) { pc = off; break; }
        if (!special && (off = find_label(sys_blob, key)) != 0) {
            restore_file = cur_file; switch_file(scn_sys); pc = off; break;
        }
        /* commands specific to the DOS demake */
        if (!strcmp(key, "volume") || !strcmp(key, "sound volume")) {
            volume_menu();
            continue;
        }
        if ((!strcmp(key, "menu") || !strcmp(key, "title")) && cur_file != (int)scn_sg0) {
            con_print("Return to the title screen? Unsaved progress will be lost.");
            if (ask_yn("Return to title (y/N)? ", 0)) {
                con_clear();
                restart();
                return;
            }
            con_print("Canceled.");
            continue;
        }
        if ((off = find_label(cur_blob, "*")) != 0) { pc = off; break; }
        if (!special && (off = find_label(sys_blob, "*")) != 0) {
            restore_file = cur_file; switch_file(scn_sys); pc = off; break;
        }
    }
    con_clear();
}

static int eval_expr(u8 far *e, int len)
{
    s16 st[16];
    int sp = 0, i = 0;
    while (i < len) {
        u8 op = e[i++];
        switch (op) {
        case E_VAR: {
            u8 ns = e[i]; u16 idx = e[i + 1] | (e[i + 2] << 8);
            i += 3;
            st[sp++] = (ns < 3 && idx < nvar[ns]) ? vars[ns][idx] : 0;
            break; }
        case E_CONST: st[sp++] = (s16)(e[i] | (e[i + 1] << 8)); i += 2; break;
        case E_EQ:  sp--; st[sp - 1] = st[sp - 1] == st[sp]; break;
        case E_NE:  sp--; st[sp - 1] = st[sp - 1] != st[sp]; break;
        case E_AND: sp--; st[sp - 1] = st[sp - 1] && st[sp]; break;
        case E_OR:  sp--; st[sp - 1] = st[sp - 1] || st[sp]; break;
        case E_NOT: st[sp - 1] = !st[sp - 1]; break;
        case E_PREFIX: {
            u8 n = e[i++]; int k, ok = 1;
            for (k = 0; k < n; k++)
                if (tolower((u8)last_input[k]) != e[i + k]) { ok = 0; break; }
            i += n;
            st[sp++] = (s16)ok;
            break; }
        default: return 0;
        }
        if (sp >= 15) return 0;
    }
    return sp ? st[sp - 1] : 0;
}

static void text_op(u8 far *s, u16 n)
{
    static Tok t[1024];
    u16 i, m = 0;
    int voice = 0;
    for (i = 0; i < n && m < 1024 - MAX_INPUT; i++) {
        if (s[i] == 1) {                        /* last command entered */
            char *p = last_input;
            while (*p && m < 1024 - 1) { t[m].zen = 0; t[m].v = (u8)*p++; m++; }
        } else if (s[i] == 2 && i + 1 < n) {    /* voice of the speaking character */
            voice = s[++i] - '0';
        } else if (s[i] == 3 && i + 1 < n) {    /* full-width character */
            t[m].zen = 1; t[m].v = s[++i]; m++;
        } else { t[m].zen = 0; t[m].v = s[i]; m++; }
    }
    text_line(t, m, 1, voice);
}

static void goto_scenario(int id)
{
    memset(vars[1], 0, nvar[1] * 2);
    switch_file(id);
    pc = code_start(cur_blob);
}

static void restart(void)
{
    music_stop();
    memset(vars[0], 0, nvar[0] * 2);
    con_clear();
    restore_file = -1;
    f_prompt[0] = 0;
    strcpy(last_prompt, "command? ");
    goto_scenario(scn_sg0);
}

static void run(void)
{
    for (;;) {
        u8 far *b = cur_blob;
        u8 op = b[pc++];
        u16 a;
        switch (op) {                            /* a text line waits for a key before what follows */
        case OP_DRAW: case OP_PUSHDRAW: case OP_POPDRAW: case OP_CLEAR: case OP_WAIT:
        case OP_GOTO: case OP_GAMEOVER: case OP_RESTART: case OP_SE:
            text_ack();
            break;
        case OP_WAITCLICK:
            text_pending = 0;
            break;
        }
        switch (op) {
        case OP_TEXT:
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            text_op(b + pc, a); pc += a;
            break;
        case OP_DRAW:
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            paint_graphic(a);
            break;
        case OP_PUSHDRAW:
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            pushed_graphic = last_graphic;
            paint_graphic(a);
            break;
        case OP_POPDRAW:
            show_graphic(pushed_graphic);
            break;
        case OP_CLEAR:
            last_graphic = -1;
            clear_image();
            break;
        case OP_WAIT:
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            wait_ms(a);
            break;
        case OP_WAITCLICK:
            wait_click();
            break;
        case OP_INPUT:
            if (b[pc++] == 0) {
                do_input(f_prompt[0] ? f_prompt : "command? ");
            } else {
                char p[64]; u8 n = b[pc++];
                _fmemcpy(p, b + pc, n); p[n] = 0; pc += n;
                do_input(p);
            }
            break;
        case OP_PROMPT: {
            u8 n = b[pc++];
            _fmemcpy(f_prompt, b + pc, n); f_prompt[n] = 0; pc += n;
            break; }
        case OP_SET: {
            u8 ns = b[pc]; u16 idx = b[pc + 1] | (b[pc + 2] << 8);
            s16 v = (s16)(b[pc + 3] | (b[pc + 4] << 8));
            pc += 5;
            if (ns < 3 && idx < nvar[ns]) {
                vars[ns][idx] = v;
                if (ns == 2) save_sysflags();
            }
            break; }
        case OP_IF: {
            u8 n = b[pc++];
            int r = eval_expr(b + pc, n);
            pc += n;
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            if (!r) pc = code_start(b) + a;
            break; }
        case OP_JMP:
            a = b[pc] | (b[pc + 1] << 8);
            pc = code_start(b) + a;
            break;
        case OP_GOTO:
            a = b[pc] | (b[pc + 1] << 8);
            restore_file = -1;
            goto_scenario(a);
            break;
        case OP_DONE:
            do_done();
            break;
        case OP_SAVEASK: do_slot_ask(1); break;
        case OP_LOADASK: do_slot_ask(0); break;
        case OP_SAVE: do_save(b[pc]); pc++; break;
        case OP_LOAD: do_load(b[pc]); pc++; break;
        case OP_GAMEOVER:
            a = b[pc] | (b[pc + 1] << 8); pc += 2;
            paint_graphic(a);
            wait_click();
            restart();
            break;
        case OP_RESTART:
            restart();
            break;
        case OP_END:
            pc--;
            do_input(last_prompt);
            break;
        case OP_PLAY: music_play(b[pc++]); break;
        case OP_STOP: music_stop(); break;
        case OP_SE: sfx_play(b[pc++]); break;
        case OP_TITLE: {                          /* shown at once, then a page wait */
            static Tok t[64];
            const char far *m = music_title(b[pc++]);
            int n = 0;
            while (m[n] && n < 60) { t[n].zen = 0; t[n].v = (u8)m[n]; n++; }
            text_line(t, n, 0, 0);
            break; }
        default:
            fatal("SG8.SCN is damaged (unknown opcode)");
        }
    }
}

/* ------------------------------------------------------------------ startup */
static void quit_prompt(void)
{
    const char *q = "Quit game? (y/N)";
    int i, k, fk = fkey_visible, sh = fkey_shift;
    vga_clear_rows(FK_Y, 16);
    for (i = 0; q[i]; i++) put_char(TX_COL0 + i, FK_Y, (u8)q[i], C_WHITE, C_BLACK);
    k = getkey();
    if (k == 'y' || k == 'Y') quit_game(0);
    vga_clear_rows(FK_Y, 16);
    if (fk) fkey_draw(sh);
}

static void quit_game(int code)
{
    set_mode(3);
    printf("El Psy Kongroo.\n");
    exit(code);
}

/* The PC-8801 start-up screen of the original (boot_pc8801.ks), full screen. */
static void boot_screen(void)
{
    static const char *lines[] = {
        "How Many Files(0-15)?",
        "5pb. FG Basic version 2.04",
        "Copyright (C) 2011 FUTURE GADGET LABORATORY",
        "1048596 Bytes free",
        "Demake by coffee.crisp",
        "Ok",
    };
    int i, k, y = 0;
    u32 t;
    for (i = 0; i < 6; i++, y += TX_LH)
        for (k = 0; lines[i][k]; k++) put_char(k, y, (u8)lines[i][k], C_WHITE, C_BLACK);
    for (i = 0; i < 6; i++) {
        fill_bytes(0, y, 1, 16, (i & 1) ? C_BLACK : C_WHITE);
        t = now_ms() + 300;
        while (now_ms() < t) if (keyready()) { getkey(); i = 6; break; }
    }
    clear_image();
}

static void usage(void)
{
    printf("SG8 - SG Variant Space Octet DOS - by coffee.crisp\n"
           "  SG8 [options]\n"
           "  /V:n    picture drawing: 0 instant, 1 fast, 2 PC-8801 (default), 3 slow machines\n"
           "  /C:n    text speed in ms per letter (default 25, 0 = instant)\n"
           "  /N      no typing beep\n"
           "  /M      no music (sound effects kept)\n"
           "  /Q      no sound at all\n"
           "  /ADLIB  music on an AdLib / Sound Blaster (3 voices); /SPEAKER PC speaker\n"
           "  /O:n    play n octaves higher (0-3), useful on a piezo beeper\n"
           "  /FLAT   no darkened scanlines\n"
           "  /GREY   grey palette (plasma or mono screens; chosen automatically)\n"
           "  /COLOUR colour palette (chosen automatically on colour screens)\n"
           "  /GREEN, /AMBER  green or amber monochrome monitor\n"
           "Settings are read from SG8.CFG (run SETUP); options override them.\n"
           "  /G:n    grey gamma (e.g. /G:1.4 brighter, /G:0.8 darker)\n"
           "In game: type commands (look, talk, save, load, help, menu, volume...)\n"
           "  F1-F5: left/back/front/right/phone   Shift+F1-F5: load/save/look/talk/assistant\n"
           "  Up arrow: last command   Esc: clear line   F9: volume   F10: quit\n");
}

static int opt_is(const char *a, const char *name)
{
    while (*name) if (toupper((u8)*a++) != *name++) return 0;
    return *a == 0;
}

int main(int argc, char **argv)
{
    int i;
    load_config();
    if (opt_render_smooth) opt_scan = 0;
    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if (a[0] == '/' || a[0] == '-') a++;
        if (opt_is(a, "FLAT")) opt_scan = 0;
        else if (opt_is(a, "GREY") || opt_is(a, "GRAY")) opt_colour = 0;
        else if (opt_is(a, "COLOUR") || opt_is(a, "COLOR")) opt_colour = 1;
        else if (toupper((u8)a[0]) == 'G' && a[1] == ':') opt_gamma = atof(a + 2);
        else if (opt_is(a, "GREEN")) opt_colour = 2;
        else if (opt_is(a, "AMBER")) opt_colour = 3;
        else if (opt_is(a, "ADLIB")) opt_sound = SND_ADLIB;
        else if (opt_is(a, "SPEAKER")) opt_sound = SND_SPEAKER;
        else if (opt_is(a, "Q")) opt_sound = SND_NONE;
        else if (opt_is(a, "M")) opt_music = 0;
        else if (opt_is(a, "N")) opt_chse = 0;
        else if (toupper((u8)a[0]) == 'C' && a[1] == ':') opt_chms = atoi(a + 2) < 0 ? 0 : atoi(a + 2);
        else if (toupper((u8)a[0]) == 'V' && a[1] == ':') opt_speed = atoi(a + 2) > 3 ? 3 : atoi(a + 2);
        else if (toupper((u8)a[0]) == 'O' && a[1] == ':') opt_octave = atoi(a + 2) & 3;
        else { usage(); return 0; }
    }
    if (opt_gamma < 0.3 || opt_gamma > 3.0) opt_gamma = 1.0;
    if (opt_speed < 0) opt_speed = 0;

    open_data();
    sys_blob = load_blob(scn_sys);
    load_sysflags();
    load_font();
    open_vec();
    sound_init();

    set_mode(0x12);
    require_vga();
    set_palette();
    con_clear();
    clear_image();

    boot_screen();
    goto_scenario(scn_sg0);
    run();
    return 0;
}
