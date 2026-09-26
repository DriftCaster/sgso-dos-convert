/*
 * INSTALL.EXE - checks the pieces copied from the floppies and rebuilds the game files.
 * The list of pieces and results is read from INSTALL.LST, written by the convert tool:
 *   P <name> <size> <crc32 hex> <disk>
 *   O <name> <size> <crc32 hex> <first piece> <piece count> <method>
 * A result is its pieces joined together; method 1 means the joined data is LZSS
 * compressed (see lib/lzss.py) and is expanded here.
 * Build: wcl -bt=dos -ms -0 -ox install.c -fe=INSTALL.EXE
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned long u32;
typedef struct { char name[13]; u32 size, crc; int disk; } Piece;
typedef struct { char name[13]; u32 size, crc; int first, count, method; } Out;

#define MAXP 80
#define MAXO 16
static Piece pieces[MAXP];
static Out outs[MAXO];
static int np, no;
static u32 table[256];
static unsigned char buf[8192];

static void mktable(void)
{
    u32 c; int i, k;
    for (i = 0; i < 256; i++) {
        c = (u32)i;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
}

/* 0 ok, 1 missing, 2 size, 3 content */
static int check(const char *name, u32 size, u32 crc)
{
    FILE *f = fopen(name, "rb");
    u32 c = 0xFFFFFFFFUL, total = 0;
    size_t n, i;
    if (!f) return 1;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (i = 0; i < n; i++) c = table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
        total += n;
    }
    fclose(f);
    if (total != size) return 2;
    return (c ^ 0xFFFFFFFFUL) == crc ? 0 : 3;
}

static int load_list(void)
{
    FILE *f = fopen("INSTALL.LST", "r");
    char kind[4], name[20];
    u32 size, crc;
    int a, b, c;
    if (!f) return 0;
    while (fscanf(f, "%3s %19s %lu %lx", kind, name, &size, &crc) == 4) {
        if (kind[0] == 'P' && np < MAXP && fscanf(f, "%d", &a) == 1) {
            strncpy(pieces[np].name, name, 12);
            pieces[np].size = size; pieces[np].crc = crc; pieces[np].disk = a; np++;
        } else if (kind[0] == 'O' && no < MAXO && fscanf(f, "%d %d %d", &a, &b, &c) == 3) {
            strncpy(outs[no].name, name, 12);
            outs[no].size = size; outs[no].crc = crc; outs[no].first = a; outs[no].count = b;
            outs[no].method = c; no++;
        }
    }
    fclose(f);
    return np > 0;
}

static const char *msg[] = { "OK", "MISSING", "WRONG SIZE", "DAMAGED" };

/* Reads the pieces of one result one after the other as a single stream. */
static FILE *rd_f;
static int rd_next, rd_last;
static unsigned char rbuf[4096];
static size_t rd_len, rd_pos;

static int rd_byte(void)
{
    while (rd_pos >= rd_len) {
        if (rd_f) { rd_len = fread(rbuf, 1, sizeof rbuf, rd_f); rd_pos = 0; if (rd_len) break; fclose(rd_f); rd_f = NULL; }
        if (rd_next > rd_last) return -1;
        rd_f = fopen(pieces[rd_next++].name, "rb");
        if (!rd_f) return -1;
        rd_len = rd_pos = 0;
    }
    return rbuf[rd_pos++];
}

static unsigned char ring[4096];
static unsigned char obuf[4096];
static size_t olen;

static int out_byte(FILE *o, int c)
{
    obuf[olen++] = (unsigned char)c;
    if (olen == sizeof obuf) {
        if (fwrite(obuf, 1, olen, o) != olen) return 0;
        olen = 0;
    }
    return 1;
}

/* Writes result i; returns 0 on a write error, -1 on a data error. */
static int assemble(FILE *o, int i)
{
    u32 done = 0;
    int c, lo, hi, k, len;
    unsigned r = 0, dist, flags = 0, bit = 8;
    rd_f = NULL; rd_next = outs[i].first; rd_last = outs[i].first + outs[i].count - 1;
    rd_len = rd_pos = 0; olen = 0;
    while (done < outs[i].size) {
        if (outs[i].method == 0) {
            if ((c = rd_byte()) < 0) return -1;
            if (!out_byte(o, c)) return 0;
            done++;
            continue;
        }
        if (bit == 8) {
            if ((c = rd_byte()) < 0) return -1;
            flags = (unsigned)c; bit = 0;
        }
        if (flags & (1u << bit)) {
            if ((c = rd_byte()) < 0) return -1;
            ring[r] = (unsigned char)c; r = (r + 1) & 4095;
            if (!out_byte(o, c)) return 0;
            done++;
        } else {
            if ((lo = rd_byte()) < 0 || (hi = rd_byte()) < 0) return -1;
            dist = (unsigned)(lo | ((hi >> 4) << 8)) + 1;
            len = (hi & 15) + 3;
            for (k = 0; k < len && done < outs[i].size; k++) {
                c = ring[(r - dist) & 4095];
                ring[r] = (unsigned char)c; r = (r + 1) & 4095;
                if (!out_byte(o, c)) return 0;
                done++;
            }
        }
        bit++;
    }
    if (rd_f) fclose(rd_f);
    if (olen && fwrite(obuf, 1, olen, o) != olen) return 0;
    return 1;
}

int main(void)
{
    int i, j, r, bad = 0, done = 1;
    mktable();
    printf("SG Variant Space Octet DOS - installation\n\n");
    if (!load_list()) {
        printf("INSTALL.LST not found: copy all the floppies into this folder first.\n");
        return 1;
    }
    for (i = 0; i < no; i++)
        if (check(outs[i].name, outs[i].size, outs[i].crc)) done = 0;
    for (i = 0; i < np; i++) {
        int is_part = 0;
        for (j = 0; j < no; j++)
            if (i >= outs[j].first && i < outs[j].first + outs[j].count) is_part = 1;
        if (is_part && done) continue;           /* already assembled */
        printf("%-12s (disk %d) ... ", pieces[i].name, pieces[i].disk);
        r = check(pieces[i].name, pieces[i].size, pieces[i].crc);
        printf("%s\n", msg[r]);
        if (r) bad = 1;
    }
    if (bad) {
        printf("\nCopy the failed disk(s) again (COPY A:*.* into this folder),\n"
               "then run INSTALL again.\n");
        return 1;
    }
    if (!done) {
        for (i = 0; i < no; i++) {
            FILE *o;
            printf("%s %s...", outs[i].method ? "Expanding" : "Writing", outs[i].name);
            o = fopen(outs[i].name, "wb");
            if (!o) { printf(": cannot write (disk full?)\n"); return 1; }
            j = assemble(o, i);
            if (j <= 0) {
                fclose(o);
                printf(j < 0 ? ": data error, copy the disks again\n" : ": disk full\n");
                return 1;
            }
            fclose(o);
            r = check(outs[i].name, outs[i].size, outs[i].crc);
            printf(" %s\n", msg[r]);
            if (r) { printf("Write error on the hard disk.\n"); return 1; }
            fflush(stdout);
        }
        for (i = 0; i < no; i++)
            for (j = 0; j < outs[i].count; j++) remove(pieces[outs[i].first + j].name);
    }
    printf("\nInstallation complete. Run SETUP to choose screen and sound, then SG8 to play.\n");
    return 0;
}
