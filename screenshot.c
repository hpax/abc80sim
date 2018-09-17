/*
 * Take a PNG screenshot of an SDL surface
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_PNG_H
# include <png.h>
#else
# error "png.h not found"
#endif

#include "screenshot.h"

static inline void *pixel_row(const SDL_Surface *surf, size_t y)
{
    return (uint8_t *)surf->pixels + (y * surf->pitch);
}

struct sort_pixel {
    uint32_t pix;			/* Pixel value */
    uint32_t pos;			/* Position index */
};

static int sort_by_pixel(const void *vp1, const void *vp2)
{
    const uint32_t pix1 = ((const struct sort_pixel *)vp1)->pix;
    const uint32_t pix2 = ((const struct sort_pixel *)vp2)->pix;

    /* A simple subtract here risks overflowing an int */
    return -(pix1 < pix2) | (pix1 > pix2);
}

/* 32-bit pixel format; the exact aspects of which are arbitrary */
static const SDL_PixelFormat argbfmt = {
    NULL,			/* palette */
    32,				/* bits per pixel */
    4,				/* bytes per pixel */
    0, 0, 0, 0,			/* precision loss (8 = all alpha lost) */
    16,				/* Rshift */
    8,				/* Gshift */
    0,				/* Bshift */
    24,				/* Ashift */
    0x00ff0000,			/* Rmask */
    0x0000ff00,			/* Gmask */
    0x000000ff,			/* Bmask */
    0xff000000,			/* Amask */
    0,				/* No actual color key */
    255				/* Completely opaque */
};

#define MAX_PALETTE 256
/* Returns the number of indicies, or -1 on failure */
static int
make_indexed(SDL_Surface *surf, uint8_t **data, png_color **palettep)
{
    struct sort_pixel *pixp = NULL;	/* Pixel pointers */
    struct sort_pixel *ppp;		/* Pixel pointer pointer */
    const uint32_t *pvp;		/* Pixel value pointer */
    uint32_t pos;			/* Current position index */
    size_t np;				/* Total number of pixels */
    size_t i;
    int x, y;
    int last_index;			/* Last allocated index */
    uint32_t last_pixel;		/* Last equivalent pixel */
    SDL_PixelFormat *fmt;		/* Cached for speed */
    SDL_PixelFormat cfmt;
    bool surface_copy = false;		/* We copied the surface */
    uint8_t *iimg = NULL;		/* Actual indexed image */
    png_color *palette = NULL;

    /* This is kind of an idiotic algorithm, but it works and is kind of fun */

    /* If this not a 32-bit surface, make it one */
    if (surf->format->BytesPerPixel != 4) {
      cfmt = argbfmt;
      surface_copy = true;	/* We created a new surface, need to free it */
      surf = SDL_ConvertSurface(surf, &cfmt, SDL_SWSURFACE);
      if (!surf)
	goto err;
    }

    /* 1. Allocate arrays and initialize the position array */
    np = surf->w * surf->h;	/* Total pixels */

    iimg = malloc(np);
    if (!iimg)
	goto err;
    pixp = malloc(np * sizeof *pixp);
    if (!pixp)
	goto err;
    palette = calloc(MAX_PALETTE, sizeof *palette);
    if (!palette)
	goto err;

    SDL_LockSurface(surf);
    ppp = pixp;
    pos = 0;
    for (y = 0; y < surf->h; y++) {
	pvp = pixel_row(surf, y);
	for (x = 0; x < surf->w; x++) {
	    ppp->pix = *pvp++;
	    ppp->pos = pos++;
	    ppp++;
	}
    }
    SDL_UnlockSurface(surf);

    /* 2. Sort by pixel value */
    qsort(pixp, np, sizeof *pixp, sort_by_pixel);

    /* 3. Create palette and index values */
    fmt = surf->format;
    ppp = pixp;
    last_pixel = ~ppp->pix;	/* Make sure we don't match on the first */
    last_index = -1;
    for (i = 0; i < np; i++) {
	if (ppp->pix != last_pixel) {
	    last_pixel = ppp->pix;
	    last_index++;
	    if (last_index >= MAX_PALETTE)
		goto err;
	    SDL_GetRGB(last_pixel, fmt,
		       &palette[last_index].red,
		       &palette[last_index].green,
		       &palette[last_index].blue);
	}
	iimg[ppp->pos] = last_index;
	ppp++;
    }

    /* Done! */
    last_index++;		/* Convert to a count */
common_exit:
    *data = iimg;
    *palettep = palette;
    if (pixp)
	free(pixp);
    if (surface_copy && surf)
      SDL_FreeSurface(surf);
    return last_index;

err:
    if (palette) {
	free(palette);
	palette = NULL;
    }
    if (iimg) {
	free(iimg);
	iimg = NULL;
    }
    last_index = -1;		/* Return -1 */
    goto common_exit;
}

/*
 * Open a screenshot file for writing
 */
#ifndef O_BINARY
# define O_BINARY 0
#endif

static FILE *open_screenshot(char *namebuf)
{
    int fd;
    unsigned int n;
    FILE *f;

    for (n = 1; n <= 9999; n++) {
	snprintf(namebuf, PATH_MAX, "scrn%04u.png", n);
	fd = open(namebuf, O_CREAT|O_EXCL|O_WRONLY|O_BINARY,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

	if (fd >= 0 || errno != EEXIST) {
	    break;
	}
    }

    if (fd < 0)
	return NULL;

    f = fdopen(fd, "wb");
    if (!f) {
	int err = errno;
	close(fd);
	errno = err;
	return NULL;
    }

    return f;
}

static void my_png_error(png_structp png, png_const_charp errmsg)
{
    (void)errmsg;
    longjmp(png_jmpbuf(png), 1);
}

static void my_png_warning(png_structp png, png_const_charp warnmsg)
{
    (void)png;
    (void)warnmsg;
}

/* This is the pixel format that PNG uses in 8-bit RGB mode */
static const SDL_PixelFormat rgbfmt = {
    NULL,			/* palette */
    24,				/* bits per pixel */
    3,				/* bytes per pixel */
    0, 0, 0, 8,			/* precision loss (8 = all alpha lost) */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    0,				/* Rshift */
    8,				/* Gshift */
    16,				/* Bshift */
    0,				/* Ashift */
    0x000000ff,			/* Rmask */
    0x0000ff00,			/* Gmask */
    0x00ff0000,			/* Bmask */
#else
    16,				/* Rshift */
    8,				/* Gshift */
    0,				/* Bshift */
    0,				/* Ashift */
    0x00ff0000,			/* Rmask */
    0x0000ff00,			/* Gmask */
    0x000000ff,			/* Bmask */
#endif
    0x00000000,			/* Amask */
    0,				/* No actual color key */
    255				/* Completely opaque */
};

/*
 * This is a bit of a hack to work around potentially dangerous
 * setjmp() side effects.  The structure contains anything that
 * we may have to deallocate or clean up.
 */
struct allocable {
    uint8_t *img;		/* Image data */
    SDL_Surface *rgbsurf;	/* RGB converted surface */
    png_bytepp rowptrs;		/* Array of row pointers */
    png_color *palette;		/* Palette data */
    png_structp png;		/* PNG write structure */
    png_infop png_info;		/* PNG info structure */
    FILE *f;			/* File pointer to screenshot file */
    char filename[PATH_MAX];	/* Filename (to remove on failure) */
};

static int do_screenshot(SDL_Surface *surf, struct allocable *a)
{
    uint8_t *row;
    size_t bytes_per_row;
    int y;
    int npalette, depth;
    time_t now;
    png_time png_now;
    SDL_PixelFormat fmt;
    png_bytepp rowptr;

    /* Get current time for timestamp */
    time(&now);
    png_convert_from_time_t(&png_now, now);

    /* Allocate row pointers */
    a->rowptrs = malloc(surf->h * sizeof *a->rowptrs);
    if (!a->rowptrs)
	return -1;

    /* First, try an indexed image */
    npalette = make_indexed(surf, &a->img, &a->palette);
    if (npalette > 0) {
	if (npalette <= 2)
	    depth = 1;
	else if (npalette <= 4)
	    depth = 2;
	else if (npalette <= 16)
	    depth = 4;
	else
	    depth = 8;

	bytes_per_row = surf->w;
	row = a->img;
    } else {
	/* Otherwise create an RGB image */
	fmt = rgbfmt; /* Is this really needed? */
	a->rgbsurf = SDL_ConvertSurface(surf, &fmt, SDL_SWSURFACE);
	if (!a->rgbsurf)
	    return -1;
	SDL_LockSurface(a->rgbsurf);
	depth = 8;
	bytes_per_row = a->rgbsurf->pitch;
	row = a->rgbsurf->pixels;
    }

    /* Generate row pointers */
    rowptr = a->rowptrs;
    for (y = 0; y < surf->h; y++) {
	*rowptr++ = row;
	row += bytes_per_row;
    }

    /* Create a PNG write and info structures */
    a->png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
				     my_png_error, my_png_warning);
    if (!a->png)
	return -1;

    a->png_info = png_create_info_struct(a->png);
    if (!a->png_info)
	return -1;

    if (setjmp(png_jmpbuf(a->png)))
	return -1;

    /* Open screenshot file */
    a->f = open_screenshot(a->filename);
    if (!a->f)
	return -1;
    png_init_io(a->png, a->f);

    /* IHDR configuration */
    png_set_IHDR(a->png, a->png_info, surf->w, surf->h, depth,
		 (npalette > 0) ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

    png_set_tIME(a->png, a->png_info, &png_now);

    if (npalette > 0)
	png_set_PLTE(a->png, a->png_info, a->palette, npalette);

    png_set_rows(a->png, a->png_info, a->rowptrs);

    png_write_png(a->png, a->png_info,
		  (depth < 8) ? PNG_TRANSFORM_PACKING : PNG_TRANSFORM_IDENTITY,
		  NULL);
    return 0;
}

int screenshot(SDL_Surface *surf)
{
    struct allocable a;
    int rv, err;

    memset(&a, 0, sizeof a);
    rv = do_screenshot(surf, &a);

    err = errno;
    if (a.rgbsurf) {
	SDL_UnlockSurface(a.rgbsurf);
	SDL_FreeSurface(a.rgbsurf);
    }
    if (a.png)
	png_destroy_write_struct(&a.png, &a.png_info);
    if (a.f)
	fclose(a.f);
    if (a.filename[0] && rv)
	remove(a.filename);
    if (a.palette)
	free(a.palette);
    if (a.img)
	free(a.img);
    if (a.rowptrs)
	free(a.rowptrs);

    errno = err;
    return rv;
}
