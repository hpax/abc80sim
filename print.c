/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2018 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * print.c
 *
 * abcprintd backend - also usable for abc80sim
 *
 */

#include "compiler.h"
#include "abcprintd.h"
#include "hostfile.h"
#include "print.h"

#include <wchar.h>
#include <locale.h>

#ifdef __WIN32__
const char *lpr_command =
    "powershell -windowstyle hidden -nologo -command \"get-content -literalpath '*' | out-printer\"";
#else
const char *lpr_command = "lpr '*'";
#endif

static void print_finish(struct abcprint *me)
{
    const char *p;
    char *cmd, *q;
    size_t cmdlen, namelen;
    struct host_file *hf = me->prfile;

    if (!hf)
        return;

    fflush(hf->f);

    namelen = hf->namelen;
    cmdlen = 0;
    for (p = lpr_command; *p; p++) {
        cmdlen += (*p == '*') ? namelen : 1;
    }
    cmd = malloc(cmdlen + 1);

    if (cmd) {
        for (p = lpr_command, q = cmd; *p; p++) {
            if (*p == '*') {
                memcpy(q, hf->filename, namelen);
                q += namelen;
            } else {
                *q++ = *p;
            }
        }
        *q = '\0';

        system(cmd);
        free(cmd);
    }
    close_file(&me->prfile);
}

static void output(struct abcprint *me, unsigned char c)
{
    static const char temp_prefix[] = "abcprint_tmp_";
    struct host_file *hf = me->prfile;

    static const wchar_t abc_to_unicode[256] =
        L"\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017"
        L"\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"
        L" !\"#¤%&\'()*+,-./0123456789:;<=>?"
        L"ÉABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÅÜ_"
        L"éabcdefghijklmnopqrstuvwxyzäöåü\x25a0"
        L"\x20ac\x25a1\x201a\x0192\x201e\x2026\x2020\x2021"
        L"\x02c6\x2030\x0160\x2039\x0152\x2190\x017d\x2192"
        L"\x2191\x2018\x2019\x201c\x201d\x2022\x2013\x2014"
        L"\x02dc\x2122\x0161\x203a\x0153\x2193\x017e\x0178"
        L"\240\241\242\243$\245\246\247\250\251\252\253\254\255\256\257"
        L"\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
        L"\300\301\302\303[]\306\307\310@\312\313\314\315\316\317"
        L"\320\321\322\323\324\325\\\327\330\331\332\333^\335\336\337"
        L"\340\341\342\343{}\346\347\350`\352\353\354\355\356\357"
        L"\360\361\362\363\364\365|\367\370\371\372\373~\375\376\377";

    if (hf) {
        if (c < '\b' || (c > '\r' && c < 31))
            me->prfile = hf = temp_file(HF_BINARY, temp_prefix);
        else
            me->prfile = hf = temp_file(HF_UNICODE, temp_prefix);
    }

    if (hf->mode == HF_BINARY)
        putc(c, hf->f);
    else if (c != '\r')
        putwc(abc_to_unicode[c], hf->f);
}

FILE *console_file;

void abcprint_reset(struct abcprint *me)
{
    fileop_reset(me);
    me->istate = is_normal;
}

struct abcprint *abcprint_init(send_func send_data, void *pvt)
{
    struct abcprint *me = calloc(sizeof *me, 1);
    if (!me)
	return NULL;

    me->sd.func = send_data;
    me->sd.pvt = pvt;
    abcprint_reset(me);
    return me;
}

void abcprint_recv(struct abcprint *me, const void *data, size_t len)
{
    const unsigned char *dp = data;
    unsigned char c;

    while (len--) {
        c = *dp++;

        switch (me->istate) {
        case is_normal:
            if (c == 0xff)
                me->istate = is_ff;
            else
                output(me, c);
            break;

        case is_ff:
            if (c == 0) {
                /* End of job */
                print_finish(me);
                me->istate = is_normal;
            } else if (c >= 0xa0 && c <= 0xbf) {
                /* Opcode range reserved for file ops */
                me->istate = file_op(me, c) ? is_file : is_normal;
            } else if (c == 0xc0) {
                /* Output to console */
                me->istate = is_console;
            } else {
                output(me, c);
                me->istate = is_normal;
            }
            break;

        case is_file:
            me->istate = file_op(me, c) ? is_file : is_normal;
            break;

        case is_console:
            if (c == 0) {
                me->istate = is_normal;
                if (console_file)
                    fflush(console_file);
            } else if (console_file) {
                fputc(c, console_file);
            }
            break;
        }
    }
}
