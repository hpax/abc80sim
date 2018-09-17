/*
 * Take a PNG screenshot of an SDL surface
 * Currently assumes the SDL surface is 32 bits
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <png.h>
#include <zlib.h>

#include "screenshot.h"

static inline void *pixel_row(const SDL_Surface *surf, size_t y)
{
    return (char *)surf->pixels + (y * surf->pitch);
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
    SDL_PixelFormat * const fmt = surf->format;	/* Cached for speed */
    uint8_t *iimg;			/* Actual indexed image */
    png_color *palette = NULL;

    /* This is kind of an idiotic algorithm, but it works and is kind of fun */

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
	pvp = (const uint32_t *)pixel_row(surf, y);
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
    *data = iimg;
    *palettep = palette;
    free(pixp);
    return last_index + 1;

err:
    if (palette)
	free(palette);
    if (pixp)
	free(pixp);
    if (iimg)
	free(iimg);
    *data = NULL;
    *palettep = NULL;
    return -1;
}

#if 0
/*
 * Create a 24-bit surface in RGB format, as used by PNG
 */
static png_color *make_rgb(const SDL_Surface *surf)
{
    int x, y;
    png_color *data, *rp;
    size_t np;
    const uint32_t *pvp, pix, last_pix;

    np = surf->w * surf->h;
    data = rp = malloc(np * 3);
    if (!data)
	return data;

    last_pix = ~*(const uint32_t *)surf->pixels; /* Don't match first time */
    for (y = 0; y < surf->h; y++) {
	pvp = ((const uint32_t *)surf->pixels) + (y * surf->w);
	for (x = 0; x < surf->w; x++) {
	    pix = *pvp++;
	    if (pix == last_pix) {
		/* Speed hack */
		*rp = rp[-1];
	    } else {
		SDL_GetRGB(pix, surf->format, &rp->red, &rp->green, &rp->blue);
		last_pix = pix;
	    }
	    rp++;
	}
    }

    return data;
}
#endif

/*
 * Open a screenshot file for writing
 */
#ifndef O_BINARY
# define O_BINARY 0
#endif

FILE *open_screenshot(const char **name)
{
    int fd;
    unsigned int n;
    static char filename[16];
    FILE *f;

    *name = NULL;

    for (n = 1; n <= 9999; n++) {
	snprintf(filename, sizeof filename, "scrn%04u.png", n);
	fd = open(filename, O_CREAT|O_EXCL|O_WRONLY|O_BINARY,
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

    *name = filename;
    return f;
}

struct my_png_error {
    jmp_buf jump;
};

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
    0, 0, 0, 0,			/* precision loss */
#if SDL_ENDIAN == SDL_LIL_ENDIAN
    0,				/* Rshift */
    8,				/* Gshift */
    16,				/* Bshift */
    24,				/* Ashift */
    0x000000ff,			/* Rmask */
    0x0000ff00,			/* Gmask */
    0x00ff0000,			/* Bmask */
    0x00000000,			/* Amask */
#else
    16,				/* Rshift */
    8,				/* Gshift */
    0,				/* Bshift */
    24,				/* Ashift */
    0x00ff0000,			/* Rmask */
    0x0000ff00,			/* Gmask */
    0x000000ff,			/* Bmask */
    0x00000000,			/* Amask */
#endif
    -1,				/* No actual color key */
    0				/* Completely opaque */
};

int screenshot(SDL_Surface *surf)
{
    FILE *f = NULL;
    int rv = -1;
    int err;
    uint8_t *img = NULL;
    SDL_Surface *rgbsurf = NULL;
    uint8_t *row;
    png_bytepp rowptrs = NULL;
    size_t bytes_per_row;
    int y;
    png_color *palette = NULL;
    int npalette, depth;
    png_structp png = NULL;
    png_infop png_info;
    const char *filename;
    time_t now;
    png_time png_now;
    SDL_PixelFormat fmt;

    /* Get current time for timestamp */
    time(&now);
    png_convert_from_time_t(&png_now, now);

    /* Allocate row pointers (why, libpng?) */
    rowptrs = malloc(surf->h * sizeof *rowptrs);
    if (!rowptrs)
	goto err;

    /* First, try an indexed image */
    bytes_per_row = surf->w;
    npalette = make_indexed(surf, &img, &palette);
    if (!palette) {
	/* Otherwise create an RGB image */
	fmt = rgbfmt; /* Is this really needed? */
	rgbsurf = SDL_ConvertSurface(surf, &fmt, SDL_SWSURFACE);
	if (!rgbsurf)
	    goto err;
	SDL_LockSurface(rgbsurf);
	img = rgbsurf->pixels;
	depth = 8;
	bytes_per_row *= 3;
    } else if (npalette <= 2) {
	depth = 1;
    } else if (npalette <= 4) {
	depth = 2;
    } else if (npalette <= 16) {
	depth = 4;
    } else {
	depth = 8;
    }

    /* Create a PNG write and info structures */
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
				  my_png_error, my_png_warning);
    if (!png)
	goto err;

    png_info = png_create_info_struct(png);
    if (!png_info)
	goto err;

    /* Open screenshot file */
    f = open_screenshot(&filename);
    if (!f)
	goto err;
    png_init_io(png, f);

    if (setjmp(png_jmpbuf(png)))
	goto err;

    /* IHDR configuration */
    png_set_IHDR(png, png_info, surf->w, surf->h, depth,
		 palette ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

    png_set_tIME(png, png_info, &png_now);

    if (palette)
	png_set_PLTE(png, png_info, palette, npalette);

    /* Why does libpng need this? */
    row = img;
    for (y = 0; y < surf->h; y++) {
	rowptrs[y] = row;
	row += bytes_per_row;
    }

    png_set_rows(png, png_info, rowptrs);

    png_write_png(png, png_info,
		  (depth < 8) ? PNG_TRANSFORM_PACKING : PNG_TRANSFORM_IDENTITY,
		  NULL);
    rv = 0;			/* Success! */

err:
    err = errno;
    if (rgbsurf) {
	SDL_UnlockSurface(rgbsurf);
	SDL_FreeSurface(rgbsurf);
    }
    if (png)
	png_destroy_write_struct(&png, &png_info);
    if (f) {
	fclose(f);
	if (rv)
	    remove(filename);
    }
    if (palette)
	free(palette);
    if (img)
	free(img);
    if (rowptrs)
	free(rowptrs);

    errno = err;
    return rv;
}
