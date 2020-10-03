/*
 * ABC80/800 simulated disk
 */

#include "compiler.h"
#include "z80.h"
#include "abcio.h"
#include "hostfile.h"
#include "trace.h"

const char *disk_path = "abcdisk";

#define NOTTHERE 0
#define READONLY 0
#define INTERLEAVE 0

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

/* Per-drive state */
struct drive_state {
    struct host_file *hf;
    char name[4];               /* Drive name */
    unsigned int sectors;
    uint8_t ilmsk, ilfac;       /* Interlacing parameters */
};

/* Per-controller state */
struct ctl_state {
    enum out_state state;
    uint8_t k[4];               /* Command bytes */
    unsigned int clustshift;
    unsigned int maxsectors;
    unsigned int c, h, s;
    unsigned int drives;        /* Number of drives present */
    bool newaddr;
    bool fmtdata_in_buf;	/* Use user-provided formatting data */
    bool trace_dump;	        /* If tracing, dump data buf after command */
    const char name[3];         /* Device type name with extra NUL */
    uint8_t ilmsk, ilfac;	/* Software interleaving parameters */
    int out_ptr;                /* Pointer within buffer for out data */
    int in_ptr;                 /* Pointer within buffer for in data */
    int error;			/* Error code */
    int notready_ctr;           /* How many times are we not ready? */
    struct drive_state drv[8];  /* Per-drive  */
    uint8_t buf[4][256];	/* 4 buffers @ 256 bytes */
};
#define NOT_READY 4		/* How many times to report not ready */

/*
 * DOSGEN depends on this value... and different DOSGEN
 * expect different values. If this value is wrong, DOSGEN
 * will spin forever on "testing sector..."
 */
#define OUT_OF_RANGE 0x21	/* Status code for an invalid sector */

static struct ctl_state mo_state = {
    .clustshift = 0,
    .maxsectors = 40 * 1 * 16,
    .c = 40, .h = 1, .s = 16,
    .fmtdata_in_buf = true,
    .ilmsk = 15,
    .ilfac = 7,
    .name = "mo"
};

static struct ctl_state mf_state = {
    .clustshift = 2,
    .maxsectors = 80 * 2 * 16,
    .c = 80, .h = 2, .s = 16,
    .name = "mf"
};

static struct ctl_state sf_state = {
    .clustshift = 2,
    .maxsectors = (77 * 2 - 1) * 26, /* Track 0, side 0 not used */
    .c = 77, .h = 2, .s = 26,
    .name = "sf"
};

static struct ctl_state hd_state = {
    .clustshift = 5,
    .newaddr = true,            /* Actually irrelevant for clustshift = 5 */
    .maxsectors = (239 * 32 - 1) * 32,     /* Maximum supported by UFD-DOS */
    .c = 238, .h = 16, .s = 64,
    .name = "hd"
};

static struct ctl_state *const sel_to_state[64] = {
    [36] = &hd_state,
    [44] = &mf_state,
    [45] = &mo_state,
    [46] = &sf_state,
};

static inline struct drive_state *cur_drv_mutable(struct ctl_state *state)
{
    return &state->drv[state->k[1] & 7];
}

static inline const struct drive_state *cur_drv(const struct ctl_state *state)
{
    return &state->drv[state->k[1] & 7];
}

static inline unsigned int cur_sector(const struct ctl_state *state)
{
    uint8_t k2 = state->k[2], k3 = state->k[3];

    if (state->newaddr)
        return (k2 << 8) + k3;
    else
        return (((k2 << 3) + (k3 >> 5)) << state->clustshift) + (k3 & 31);
}

/* Get physical sector number, after interleaving */
static inline unsigned int
virt2phys(const struct drive_state *drv, unsigned int sector)
{
    unsigned int ilmsk = drv->ilmsk;
    unsigned int ilfac = drv->ilfac;

    sector = (sector & ~ilmsk) | ((sector * ilfac) & ilmsk);
    return sector;
}

static inline unsigned int phys_sector(const struct ctl_state *state)
{
    return virt2phys(cur_drv(state), cur_sector(state));
}

static inline unsigned int file_pos(const struct ctl_state *state)
{
    return phys_sector(state) << 8;
}

static inline bool cur_sector_valid(const struct ctl_state *state)
{
    uint8_t k3 = state->k[3];

    if (!state->newaddr && ((k3 & 31) >> state->clustshift))
	return false;

    return cur_sector(state) < cur_drv(state)->sectors;
}

static inline bool file_pos_valid(const struct ctl_state *state)
{
    return file_pos(state) < cur_drv(state)->hf->filesize - 255;
}

static inline uint8_t *cur_buf(struct ctl_state *state)
{
    return state->buf[state->k[1] >> 6];
}

static void disk_reset_state(struct ctl_state *state)
{
    int i;

    state->state = disk_k0;
    state->error = 0;
    state->in_ptr = -1;
    state->out_ptr = 0;
    state->notready_ctr = NOT_READY;

    for (i = 0; i < 8; i++)
        flush_file(state->drv[i].hf);
}

static void disk_init(struct ctl_state *state)
{
    int i;

    if (!disk_path)
        return;                 /* Nowhere to get disk files */

    /* If any of these don't exist we simply report device not ready */
    for (i = 0; i < 8; i++) {
        struct drive_state *drv = &state->drv[i];
        unsigned int filesec;

        snprintf(drv->name, sizeof drv->name, "%-.2s%c", state->name, i + '0');

        /* Try open RDWR first, then RDONLY, but don't create */
        drv->hf =
            open_host_file(HF_BINARY | HF_RETRY, disk_path, drv->name, O_RDWR);

        if (!drv->hf)
	    continue; /* File not present = drive not ready */

        state->drives++;

        /*
         * Smaller than the standard disk size?  Treat the sectors
         * beyond the end as bad.
         */
	filesec = drv->hf->filesize >> 8;
        drv->sectors = (filesec && filesec < state->maxsectors)
            ? filesec : state->maxsectors;

        /* Try to memory-map the file if it seems to be an actual file */
	if (filesec)
	  map_file(drv->hf, drv->sectors << 8);

	if (tracing(TRACE_DISK)) {
	    fprintf(tracef, "%s: initialized, sectors = %u (%u clusters), %s\n",
		    drv->name, drv->sectors, drv->sectors >> state->clustshift,
		    drv->hf->map ? "memory mapped" : "not mapped");
	}

	/* Interleaving parameters */
#if INTERLEAVE
	drv->ilfac = state->ilfac;
	drv->ilmsk = state->ilmsk;
#else
	drv->ilfac = drv->ilmsk = 0;
#endif
    }

    if (tracing(TRACE_DISK)) {
	fprintf(tracef, "%s: controller initialized with %u drives\n",
		state->name, state->drives);
    }

    disk_reset_state(state);
}

static void do_next_command(struct ctl_state *state)
{
    struct drive_state *drv = cur_drv_mutable(state);
    struct host_file *hf = drv->hf;
    uint8_t *buf = cur_buf(state);

    if (state->k[0] & 0x01) {
        /* READ SECTOR */
	if (!hf) {
	    state->error = 0x80; /* Device not ready */
	} else if (!cur_sector_valid(state)) {
	    if (tracing(TRACE_DISK)) {
		fprintf(tracef, "%s: read: disk sector out of range: %u/%u (cluster %u/%u)\n",
			drv->name, cur_sector(state), drv->sectors,
			cur_sector(state) >> state->clustshift,
			drv->sectors >> state->clustshift);
	    }
	    state->error = OUT_OF_RANGE;
	} else if (!file_pos_valid(state)) {
	    state->error = 0x08; /* CRC error(?) */
	} else {
	    if (hf->map) {
		memcpy(buf, hf->map + file_pos(state), 256);
	    } else {
		clearerr(hf->f);
		fseek(hf->f, file_pos(state), SEEK_SET);
		fread(buf, 1, 256, hf->f);
		if (ferror(hf->f))
		    state->error = 0x08; /* CRC error */
	    }
	}
        state->k[0] &= ~0x01;   /* Command done */
    }
    if (state->k[0] & 0x02) {
        /* SECTOR TO HOST */
	state->in_ptr = 0;
        state->state = disk_download;
        state->k[0] &= ~0x02;   /* Command done */
        return;
    }
    if (state->k[0] & 0x04) {
        /* SECTOR FROM HOST */
        state->state = disk_upload;
        state->out_ptr = 0;
        state->k[0] &= ~0x04;   /* Command done */
        return;
    }
    if (state->k[0] & 0x08) {
        /* WRITE SECTOR */
	if (!hf) {
	    state->error = 0x80; /* Not ready */
	} else if (!file_wrok(hf)) {
	    state->error = 0x40; /* Write protect */
	} else if (!cur_sector_valid(state)) {
	    state->error = OUT_OF_RANGE;
	    if (tracing(TRACE_DISK)) {
		fprintf(tracef, "%s: write: disk sector out of range: %u/%u (cluster %u/%u)\n",
			drv->name, cur_sector(state), drv->sectors,
			cur_sector(state) >> state->clustshift,
			drv->sectors >> state->clustshift);
	    }
	} else if (!file_pos_valid(state)) {
	    state->error = 0x08; /* CRC error(?) */
	} else {
	    if (hf->map) {
		memcpy(hf->map + file_pos(state), buf, 256);
	    } else {
		clearerr(hf->f);
		fseek(hf->f, file_pos(state), SEEK_SET);
		fwrite(buf, 1, 256, hf->f);
		if (ferror(hf->f))
		    state->error = 0x20; /* Write fault */
	    }
        }
        state->k[0] &= ~0x08;   /* Command done */
    }
    if (state->k[0] & 0x10 && state->k[1] & 0x08) {
	state->out_ptr = 0;
	/* FORMAT */
	if (!drv->hf) {
	    state->error = 0x80;	/* Not ready */
	} else if (!file_wrok(hf)) {
            state->error = 0x40;	/* Write protect */
	} else {
	    unsigned int s, c0, c1, s0, s1;
	    unsigned int cylsec = state->s * state->h;
	    uint8_t data[256];
	    unsigned int fmtsec, filesec;

	    /* Sector count produced by format */
	    fmtsec = state->maxsectors;

	    /* For non-MO-drives, this seems to be internally generated */
	    memset(data, 0x40, 256);

	    if (state->fmtdata_in_buf) {
		/*
		 * MO drives put the sector image in the buffers, for
		 * backwards compatibility and to support single density.
		 *
		 * Right before the F7 header CRC opcode is a density byte;
		 * 00 for single, and 01 for double.  The data begins after
		 * a byte of FB.
		 */
		bool single = false;
		const uint8_t *p, *ep;

		ep = state->buf[1];
		for (p = state->buf[0]+1; p < ep; p++) {
		    if (*p == 0xf7)
			single = (p[-1] == 0);
		    if (*p == 0xfb)
			break;
		}

		fmtsec >>= single;

		if (*p++ == 0xfb) { /* Data block found */
		    if (single) {
			/* Really two 128-byte sectors! */
			memcpy(data, p, 128);
			memcpy(data+128, p, 128);
		    } else {
			memcpy(data, p, 256);
		    }
		}
	    }

	    /*
	     * Adjust the size of the accessible device to the smallest
	     * of the physical file and the formatted size
	     */
	    filesec = drv->hf->filesize >> 8;
	    drv->sectors = (filesec && filesec < fmtsec) ? filesec : fmtsec;

	    /*
	     * k2 and k3 contain the first and last cylinder numbers to
	     * format, inclusively.  The last cylinder may be partial due
	     * to virtual remapping, e.g. for sf floppies.
	     */

	    c0 = state->k[2];
	    s0 = c0 * cylsec;
	    c1 = state->k[3] + 1;
	    s1 = c1 * cylsec;

	    if (tracing(TRACE_DISK)) {
		fprintf(tracef, "%s: formatting cyl %u..%u, sectors %u..%u\n",
			drv->name, c0, c1-1, s0, s1-1);
	    }

	    clearerr(hf->f);
	    state->error = 0;

	    for (s = s0; s < s1; s++) {
		unsigned int ps = virt2phys(drv, s);
		if (ps >= drv->sectors) {
		    state->error |= 0x02; /* Track 0/Lost data? */
		    break;
		} else if (hf->map) {
		    memcpy(hf->map + (ps << 8), data, 256);
		} else {
		    fseek(hf->f, ps << 8, SEEK_SET);
		    fwrite(data, 1, 256, hf->f);
		}
	    }
	    if (ferror(hf->f))
		state->error |= 0x20; /* Write fault */
	}
	state->k[1] &= ~0x08;
    }

    if (!(state->k[1] & 0x38))
	state->k[0] &= ~0x10;

    if (tracing(TRACE_DISK)) {
      if (state->trace_dump) {
	trace_dump(TRACE_DISK, drv->name, buf, 256);
	state->trace_dump = false;
      }
    }
    state->state = disk_k0;
}

static void disk_reset(uint8_t sel)
{
    struct ctl_state *state = sel_to_state[sel];

    if (state && state->state != disk_need_init)
	disk_reset_state(state);
}

static void disk_out(uint8_t sel, uint16_t port, uint8_t value)
{
    struct ctl_state *state = sel_to_state[sel];

    if (!state)
        return;                 /* Not a disk drive */

    if (tracing(TRACE_DISK)) {
	fprintf(tracef, "%s:  OUT %d/%d: %02x : ",
		state->name, sel, port, value);
	fprintf(tracef, "PC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
		REG_PC, REG_BC, REG_DE, REG_HL);
    }

    if (state->state == disk_need_init)
        disk_init(state);

    if (!state->drives) {
	if (tracing(TRACE_DISK)) {
		fprintf(tracef, "%s: no drives present\n", state->name);
	}
	return;
    }

    switch (port) {
    case 0:
	state->notready_ctr = 0;

        switch (state->state) {
        case disk_k0:
        case disk_k1:
        case disk_k2:
            state->k[state->state - disk_k0] = value;
            state->state++;
            break;
        case disk_k3:
            state->k[3] = value;
            state->state = disk_k0;

            if (tracing(TRACE_DISK)) {
		uint16_t k = (state->k[1] << 8) + state->k[0];
		fprintf(tracef, "%s: cmd %02X %02X %02X %02X "
			"sect %u (phys %u, pos %u) "
			"buf %u :%s%s%s%s%s%s%s%s%s%s\n",
			cur_drv(state)->name,
			state->k[0], state->k[1], state->k[2], state->k[3],
			cur_sector(state), phys_sector(state),
			file_pos(state),
			k >> (6+8),
			(k & 0x01) ? " read" : "",
			(k & 0x02) ? " to_host" : "",
			(k & 0x04) ? " from_host" : "",
			(k & 0x08) ? " write" : "",
			((k & 0x0810) == 0x0810) ? " format" : "",
			((k & 0x1010) == 0x1010) ? " rdmark" : "",
			((k & 0x2010) == 0x2010) ? " ?cmd5" : "",
			(k & 0x20) ? " select_drive" : "",
			(k & 0x40) ? " motor_on" : "",
			(k & 0x80) ? " ?k7" : "");
		state->trace_dump |= (state->k[0] & 15) != 0;
	    }
	    state->error = 0;	/* Clear error */
	    do_next_command(state);
            break;
        case disk_upload:
            cur_buf(state)[state->out_ptr++] = value;
            if (state->out_ptr >= 256)
                do_next_command(state);
            break;
        case disk_download:
            break;
        case disk_need_init:
            abort();            /* Should never happen */
            break;
        }
        break;

    case 2:                    /* Start command */
    case 4:                    /* Reset */
	disk_reset_state(state);
        break;

    default:
        /* Nothing */
        break;
    }
}

static uint8_t disk_in(uint8_t sel, uint16_t port)
{
    struct ctl_state *state = sel_to_state[sel];
    uint8_t v = 0xff;

    if (!state)
        return 0xff;            /* Not a disk drive */

    if (state->state == disk_need_init)
        disk_init(state);

    if (!state->drives) {
	if (tracing(TRACE_DISK)) {
		fprintf(tracef, "%s: no drives present\n", state->name);
	}
        return 0xff;            /* No drives - controller not present */
    }

    switch (port) {
    case 0:
        if (state->in_ptr >= 0) {
	    state->notready_ctr = 0;
            v = state->buf[state->k[1] >> 6][state->in_ptr++];
            if (state->in_ptr >= 256) {
                state->in_ptr = -1;
                do_next_command(state);
            }
        } else {
            v = state->error;
        }
        break;

    case 1:
	v = (state->state == disk_k0) ? 0x80 : 0;
	v |= state->error ? 0 : 8;
	v |= state->notready_ctr ? 0 : 1;
	if (state->notready_ctr)
	    state->notready_ctr--;
	break;		       /* Primary status */
    default:
        v = 0xff;
	break;
    }

    if (tracing(TRACE_DISK)) {
        fprintf(tracef, "%s:  IN  %d/%d: %02x : error %02x ",
		state->name, sel, port, v, state->error);
        fprintf(tracef, "PC = %04X  BC = %04X  DE = %04X  HL = %04X\n",
                REG_PC, REG_BC, REG_DE, REG_HL);
    }
    return v;
}

static const struct abcbus_dev disk_dev = {
    .out = disk_out,
    .in  = disk_in,
    .reset = disk_reset,
    .portmask = 7
};

void disk_register_devices(void)
{
    unsigned int i;

    for (i = 0; i < 64; i++) {
	if (sel_to_state[i])
	    register_abcbus_dev(i, &disk_dev);
    }
}
