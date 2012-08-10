/*
 * ABC80 simulated disk
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "z80.h"

#define NOTTHERE 0
#define READONLY 0
#define TRACE 0

/* This is the interpretation of an "out" command */
enum out_state {
  disk_need_init,
  disk_k0,
  disk_k1,
  disk_k2,
  disk_k3,
  disk_upload,
  disk_download
};

struct ctl_state {
  enum out_state state;
  uint8_t k[4];
  unsigned int secperclust;
  unsigned int sectors;
  uint8_t ilmsk, ilfac;		/* Interlacing parameters */
  uint8_t new;			/* "New addressing" */
  const char name[3];
  int out_ptr;			/* Pointer within buffer for out data */
  int in_ptr;			/* Pointer within buffer for in data */
  int status;			/* Primary status */
  int aux_status;		/* Auxilliary status */
  int notready_ctr;		/* How many times are we not ready? */
  FILE *files[8];		/* File for this unit */
  unsigned char buf[4][256];	/* 4 buffers @ 256 bytes */
};

static struct ctl_state mo_state =
  {
    .secperclust = 1,
    .sectors     = 40*1*16,
#if 0
    .ilmsk       = 15,
    .ilfac       = 7,
#endif
    .name        = "mo"
  };
static struct ctl_state mf_state =
  {
    .secperclust = 4,
    .sectors     = 80*2*16,
    .name        = "mf"
  };
static struct ctl_state sf_state =
  {
    .secperclust = 4,
    .sectors     = 77*2*26,
    .name        = "sf"
  };
static struct ctl_state hd_state =
  {
    .secperclust = 32,
    .new         = 1,		/* Actually irrelevant when secperclust = 32 */
    .sectors     = 238*8*32,
    .name        = "hd"
  };

static struct ctl_state * const sel_to_state[64] =
  {
    [36] = &hd_state,
    [44] = &mf_state,
    [45] = &mo_state,
    [46] = &sf_state,
  };

static inline int cur_sector(struct ctl_state *state)
{
  uint8_t k2 = state->k[2], k3 = state->k[3];

  if (state->new)
    return (k2 << 8) + k3;
  else
    return (((k2 << 3) + (k3 >> 5))*state->secperclust) + (k3 & 31);
}

static inline int file_pos_valid(struct ctl_state *state)
{
  if (state->new)
    return cur_sector(state) < state->sectors;
  else
    return cur_sector(state) < state->sectors &&
      (state->k[3] & 31) < state->secperclust;
}

static inline int file_pos(struct ctl_state *state)
{
  unsigned int ilmsk = state->ilmsk;
  unsigned int sector = cur_sector(state);

  sector = (sector & ~ilmsk) | ((sector*state->ilfac) & ilmsk);

  return sector << 8;
}

static void disk_reset_state(struct ctl_state *state)
{
  state->state = disk_k0;
  state->status = state->aux_status = 0;
  state->in_ptr  = -1;
  state->out_ptr = 0;
  state->notready_ctr = 4;
}

static void disk_init(struct ctl_state *state)
{
  char filename[64];
  int i;

  /* If any of these don't exist we simply report device not ready */
  for (i = 0; i < 8; i++) {
    snprintf(filename, sizeof filename, "disk/%s%d", state->name, i);
    state->files[i] = fopen(filename, "r+b");
    if (!state->files[i])
      state->files[i] = fopen(filename, "rb"); /* Try open readonly */
  }
  disk_reset_state(state);
}

static void do_next_command(struct ctl_state *state)
{
  FILE *file = state->files[state->k[1] & 7];

  if ( state->k[0] & 0x01 ) {
    /* READ SECTOR */
    fseek(file, file_pos(state), SEEK_SET);
    fread(state->buf[state->k[1] >> 6], 1, 256, file);
    state->k[0] &= ~0x01;	/* Command done */
  }
  if ( state->k[0] & 0x02 ) {
    /* SECTOR FROM HOST */
    state->in_ptr = 0;
    state->state = disk_upload;
    state->k[0] &= ~0x02;	/* Command done */
    return;
  }
  if ( state->k[0] & 0x04 ) {
    /* SECTOR TO HOST */
    state->state = disk_download;
    state->out_ptr = 0;
    state->k[0] &= ~0x04;	/* Command done */
    return;
  }
  if ( state->k[0] & 0x08 ) {
    /* WRITE SECTOR */
    clearerr(file);
    fseek(file, file_pos(state), SEEK_SET);
    fwrite(state->buf[state->k[1] >> 6], 1, 256, file);
    if (ferror(file)) {
      state->status = 0x08; /* Error */
      state->aux_status = 0x40; /* Write protect */
    }
    state->k[0] &= ~0x08;	/* Command done */
  }
  state->state = disk_k0;
}

void disk_reset(void)
{
  int i;
  struct ctl_state *state;
  for (i = 0; i < 64; i++) {
    state = sel_to_state[i];
    if (state && state->state != disk_need_init)
      disk_reset_state(state);
  }
}

void disk_out(int sel, int port, int value)
{
  struct ctl_state *state = sel_to_state[sel];

  if (!state)
    return;			/* Not a disk drive */

  if ( state->state == disk_need_init )
    disk_init(state);

  switch ( port ) {
  case 0:
    switch ( state->state ) {
    case disk_k0 ... disk_k2:
      state->status = state->aux_status = 0;
      state->k[state->state - disk_k0] = value;
      state->state++;
      break;
    case disk_k3:
      state->status = state->aux_status = 0;
      state->k[3] = value;
      state->state = disk_k0;

#if TRACE
      printf("%s%d: command %02X %02X %02X %02X\n",
	     state->name, state->k[1] & 7,
	     state->k[0], state->k[1], state->k[2], state->k[3]);
      printf("PC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
	     REG_PC, REG_BC, REG_DE, REG_HL);
#endif 

      /* Bad drive/sector? */
      if ( !state->files[state->k[1] & 7] ) {
	state->status = 0x08;	/* Error */
	state->aux_status = 0x80; /* Device not ready */
      } else if ( !file_pos_valid(state) ) {
	state->status = 0x08;	/* Error */
	state->aux_status = 0x10; /* Seek error */
      } else {
	do_next_command(state);
      }
      break;
    case disk_upload:
      state->buf[state->k[1] >> 6][state->out_ptr++] = value;
#if TRACE
	  printf("%02X", value);
#endif
      if ( state->out_ptr >= 256 ) {
#if TRACE
      printf("\nPC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
	     REG_PC, REG_BC, REG_DE, REG_HL);
#endif
	do_next_command(state);
      }
      break;
    case disk_download:
      break;
    }
    break;

  case 2:			/* Start command */
  case 4:			/* Reset */
#if TRACE
    printf("OUT %d/%d : ", sel, port);
    printf("PC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
	   REG_PC, REG_BC, REG_DE, REG_HL);
#endif
    disk_reset();
    break;
    
  default:
    /* Nothing */
    break;
  }
}

int disk_in(int sel, int port)
{
  struct ctl_state *state = sel_to_state[sel];
  uint8_t v = 0xff;

  if (!state)
    return 0xff;		/* Not a disk drive */

  if ( state->state == disk_need_init )
    disk_init(state);

  switch ( port ) {
  case 0:
    if ( state->in_ptr >= 0 ) {
      v = state->buf[state->k[1] >> 6][state->in_ptr++];
      if ( state->in_ptr >= 256 ) {
	state->in_ptr = -1;
	do_next_command(state);
      }
    } else {
      v = state->aux_status;
    }
    break;

  case 1:			/* Controller status */
    if (state->notready_ctr) {
      state->notready_ctr--;
      v = 0x80;
    } else {
      v = 0x01 | state->status | ((state->state == disk_k0) ? 0x80 : 0);
    }
    break;

  default:
    break;
  }

#if TRACE
  printf("IN %d/%d: %02X : ", sel, port, v);
  printf("PC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
	 REG_PC, REG_BC, REG_DE, REG_HL);
#endif
  return v;
}
