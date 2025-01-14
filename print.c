/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2022 H. Peter Anvin - All Rights Reserved
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
#include "trace.h"

#include <wchar.h>
#include <locale.h>

#ifdef _WIN32
const char *lpr_command = "notepad /p?{t \"?\"} \"*\"";
#else
const char *lpr_command = "lpr ?{-P '?'} '*'";
#endif

int printer_close(struct abcprint *me, struct host_file **hfp,
		   const char *prname)
{
    const char *p;
    char *cmd, *q;
    size_t cmdlen, filelen, prlen;
    struct host_file *hf = *hfp;

    (void)me;

    if (!hf)
        return 0;

    fflush(hf->f);

    filelen = strlen(hf->filename);
    prlen = strlen(prname);

    /*
     * This will overestimate the storage needed when ?{|} is in use,
     * but that doesn't matter...
     */

    cmdlen = 1;			/* Final null */
    for (p = lpr_command; *p; p++) {
	switch (*p) {
	case '*':
	    cmdlen += filelen;
	    break;
	case '?':
	    cmdlen += prlen;
	    break;
	default:
	    cmdlen++;
	    break;
	}
    }
    cmd = malloc(cmdlen);

    if (cmd) {
	bool suppress = false;
	int bracelevel = 0;
	char lastbrace = 0;

        for (p = lpr_command, q = cmd; *p; p++) {
	    switch (*p) {
	    case '*':
		q = mempcpy(q, hf->filename, filelen);
		break;
	    case '?':
		if (p[1] == '{') {
		    lastbrace = p[1];
		    suppress = !prlen;
		    p++;
		} else if (!suppress) {
		    q = mempcpy(q, prname, prlen);
		}
		break;
	    case '{':
		bracelevel++;
		*q++ = *p;
		break;
	    case '|':
		if (lastbrace == '{' && !bracelevel) {
		    suppress = !suppress;
		    lastbrace = *p;
		    break;
		}
		*q++ = *p;
		break;
	    case '}':
		if (bracelevel) {
		    bracelevel--;
		} else if (lastbrace) {
		    suppress = false;
		    lastbrace = 0;
		    break;
		}
		*q++ = *p;
		break;
	    default:
		*q++ = *p;
		break;
	    }
	}
        *q = '\0';

        system(cmd);
        free(cmd);
    }
    close_file(hfp);
    return 0;
}

static int printer_close_default(struct abcprint *me)
{
    return printer_close(me, &me->prfile, "");
}

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

int printer_write(struct abcprint *me,
		  struct host_file **hfp, const char *prname,
		  const char *data, size_t len)
{
    struct host_file *hf = *hfp;

    (void)me;

    if (!len)
	return 0;

    if (unlikely(!hf)) {
	enum host_file_mode mode = HF_UNICODE;
	char temp_prefix[64];

	if (!data)
	    return 0;

	snprintf(temp_prefix, sizeof temp_prefix, "abcprint_pr_%s_", prname);

	for (size_t i = 0; i < len; i++) {
	    unsigned char c = data[i];
	    if (c < '\b' || (c > '\r' && c < 31)) {
		mode = HF_BINARY;
		break;
	    }
	}
	*hfp = hf = temp_file(mode, temp_prefix);
	if (!hf)
	    return 128 + 41;
    }

    if (hf->mode == HF_BINARY) {
	fwrite(data, 1, len, hf->f);
    } else {
	while (len--) {
	    unsigned char c = *data++;
#ifndef _WIN32
	    if (c == '\r')
		continue;
#endif
	    putwc(abc_to_unicode[c], hf->f);
	}
    }
    return 0;
}

static void output(struct abcprint *me, char c)
{
    printer_write(me, &me->prfile, "", &c, 1);
}

FILE *console_file;

void abcprint_reset(struct abcprint *me)
{
    fileop_reset(me);
    me->istate = is_normal;
    me->pktmode = false;	/* Allow compatibility output */
}

struct abcprint *abcprint_init(send_func send_data, void *pvt)
{
    struct abcprint *me = calloc(1, sizeof *me);
    if (!me)
	return NULL;

    me->sd.func = send_data;
    me->sd.pvt = pvt;

    fileop_init(me);
    abcprint_reset(me);
    return me;
}

void abcprint_shutdown(struct abcprint *me)
{
    if (!me)
	return;

    printer_close_default(me);
    fileop_shutdown(me);
    free(me);
}

void abcprint_recv(struct abcprint *me, const void *data, size_t len)
{
    const unsigned char *dp = data;
    unsigned char c;

    if (tracing(TRACE_PRDATA))
	trace_dump_data("PR:  ", data, len);

    while (len--) {
	enum input_state istate = me->istate;
        c = *dp++;

        switch (istate) {
        case is_normal:
	    if (c == 0xff) {
                me->istate = is_ff;
	    } else if (!me->pktmode) {
                output(me, c);
	    }
            break;

        case is_ff:
        case is_printer_ff:
	    me->istate = is_normal; /* Unless otherwise stated... */
	    switch (c) {
	    case 0x00:		/* For limited backwards compatibility */
	    case 0xfd:
		/* FF FD: End of job "done" */
		if (tracing(TRACE_PR))
		    fprintf(tracef, "PR:  FF %02X  : EOF - sending job to printer\n", c);
                printer_close_default(me);
		break;
	    case 0xff:
		/* FF FF: can be sent indefinitely to resync */
		me->istate = is_ff;
		break;
	    case 0xfa:
		/* FF FA: "Acknowledge" */
		if (tracing(TRACE_PR))
		    fprintf(tracef, "PR:  FF FA  : ENQ - replying with AF\n");

		me->sd.func(me->sd.pvt, "\xaf", 1); /* Respond with AF */
		break;
	    case 0xf3:
		/* FF F3: Printer data (terminate with FF EF) */
		if (tracing(TRACE_PR))
		    fprintf(tracef, "PR:  FF F3  : PRN - printer output\n");
		me->pktmode = true;
		me->istate = is_printer;
		break;
	    case 0xfe:
		/* FF FE: Output FF to the printer */
		if (istate == is_printer_ff || !me->pktmode)
		    output(me, 0xff);
		me->istate = istate - 1; /* ff -> normal, printer_ff -> printer */
		break;
	    case 0xc0:
		/* FF C0: Console output */
		if (tracing(TRACE_PR))
		    fprintf(tracef, "PR:  FF C0  : CON - console output\n");
		me->istate = is_console;
		break;
	    case 0xa0 ... 0xbf:
		/* Opcode range reserved for file operations */
                me->istate = file_op(me, c) ? is_file : is_normal;
		break;
	    case 0xef:
	    case 0xf0:
	    default:
		/* FF EF: End frame (return to main state) */
		/* FF F0: Do nothing ("null", return to main state) */
		if (tracing(TRACE_PR)) {
		    fprintf(tracef, "PR:  FF %02X  : %s\n", c,
			    (c == 0xef) ? "end frame" :
			    (c == 0xf0) ? "NOP" : "unknown");
		}
		break;
            }
            break;

        case is_file:
            me->istate = file_op(me, c) ? is_file : is_normal;
            break;

        case is_console:
            if (c == 0 || c == 0xff) {
                me->istate = is_normal;
                if (console_file)
                    fflush(console_file);
            } else if (console_file) {
                fputc(c, console_file);
            }
            break;

	case is_printer:
	    if (c == 0xff)
		me->istate = is_printer_ff;
	    else
		output(me, c);
	    break;
        }
    }
}
