/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2013 H. Peter Anvin - All Rights Reserved
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

#include "abcprintd.h"

#include <sys/wait.h>

int lpr_argc;
const char **lpr_argv;

enum output_state {
  os_first,			/* Brand new job */
  os_percent,			/* Job started with %, might be Postscript */
  os_binary,			/* Don't touch this */
  os_text			/* It's text, OK to modify */
};

static void print_setup(FILE **tfp, enum output_state *psp)
{
  FILE *tf = *tfp;
  pid_t f;

  if ( tf ) {
    fflush(tf);
    rewind(tf);

    f = fork();

    if ( f < 0 ) {
      perror("fork");
      exit(1);
    } else if ( f == 0 ) {
      dup2(fileno(tf), STDIN_FILENO);
      fclose(tf);
      execvp(lpr_argv[0], (char **)lpr_argv);
      _exit(255);
    } else {
      fclose(tf);
      while ( waitpid(f, NULL, 0) != f );
    }
  }

  *tfp = tmpfile();
  if ( !*tfp ) {
    perror("tmpfile");
    exit(1);
  }
  *psp = os_first;
}

static void output(int c, FILE *tf, enum output_state *psp)
{
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

  switch ( *psp ) {
  case os_first:
    if ( c == 27 )
      *psp = os_binary;
    else if ( c == '%' ) {
      *psp = os_percent;
      return;
    } else {
      *psp = os_text;
      // fwrite(text_prefix, 1, sizeof text_prefix - 1, tf);
    }
    output(c, tf, psp);
    break;

  case os_percent:
    if ( c == '!' ) {
      *psp = os_binary;
    } else {
      *psp = os_text;
      // fwrite(text_prefix, 1, sizeof text_prefix - 1, tf);
    }
    output('%', tf, psp);
    output(c, tf, psp);
    break;

  case os_text:
    if (c != '\r')
      putwc(abc_to_unicode[(unsigned char)c], tf);
    break;

  case os_binary:
    putc(c, tf);
    break;
  }
}

static FILE *tf = NULL;
static enum output_state os;

enum input_state {
  is_normal,                  /* Normal operation */
  is_ff,                      /* 0xFF received */
  is_file,                    /* File operation in progress */
};
static enum input_state is;

void abcprint_init(void)
{
  print_setup(&tf, &os);
  is = is_normal;
}

void abcprint(const void *data, size_t len)
{
  const unsigned char *dp = data;
  unsigned char c;

  while (len--) {
    c = *dp++;

    switch ( is ) {
    case is_normal:
      if ( c == 0xff )
	is = is_ff;
      else
	output(c, tf, &os);
      break;

    case is_ff:
      if ( c == 0 ) {
	print_setup(&tf, &os);	/* BREAK received, end of job */
	is = is_normal;
      } else if ( c >= 0xa0 && c <= 0xbf ) {
	/* Opcode range reserved for file ops */
	is = file_op(c) ? is_file : is_normal;
      } else {
	output(c, tf, &os);
	is = is_normal;
      }
      break;

    case is_file:
      is = file_op(c) ? is_file : is_normal;
      break;
    }
  }
}
