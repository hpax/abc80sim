/*
 * Take a PNG screenshot of an ARGB8888 framebuffer.
 *
 * Copyright (C) 2026 H. Peter Anvin <hpa@zytor.com>
 */

#include "compiler.h"
#include "screenshot.h"
#include "abcio.h"
#include "hostfile.h"

#include <png.h>
#include <zlib.h>

const char *screen_path;

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

int screenshot(const uint32_t *pixels, unsigned int width, unsigned int height,
	       const char *path)
{
    struct host_file *hf = NULL;
    png_structp png = NULL;
    png_infop png_info = NULL;
    uint8_t *row = NULL;
    time_t now;
    png_time png_now;
    unsigned int x, y;
    volatile int rv = -1;
    int err;

    row = malloc(width * 3);
    if (!row)
	goto exit;

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
				  my_png_error, my_png_warning);
    if (!png)
	goto exit;

    png_info = png_create_info_struct(png);
    if (!png_info)
	goto exit;

    if (setjmp(png_jmpbuf(png)))
	goto exit;

    hf = dump_file(HF_BINARY, path, screen_path, "scrn", ".png");
    if (!hf)
	goto exit;
    png_init_io(png, hf->f);

    time(&now);
    png_convert_from_time_t(&png_now, now);
    png_set_IHDR(png, png_info, width, height, 8, PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    png_set_tIME(png, png_info, &png_now);
    png_set_compression_level(png, Z_BEST_COMPRESSION);
    png_set_compression_strategy(png, Z_FILTERED);

    png_write_info(png, png_info);
    for (y = 0; y < height; y++) {
	const uint32_t *src = pixels + ((size_t)y * width);

	for (x = 0; x < width; x++) {
	    uint32_t pixel = src[x];

	    row[3*x+0] = pixel >> 16;
	    row[3*x+1] = pixel >> 8;
	    row[3*x+2] = pixel;
	}
	png_write_row(png, row);
    }
    png_write_end(png, png_info);

    keep_file(hf);
    rv = 0;

exit:
    err = errno;
    if (png)
	png_destroy_write_struct(&png, &png_info);
    free(row);
    close_file(&hf);
    errno = err;
    return rv;
}
