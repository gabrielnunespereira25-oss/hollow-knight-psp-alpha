
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    unsigned int *pixels; /* RGBA8888 */
} PngImage;

void png_image_free(PngImage *image)
{
    if (image != NULL) {
        free(image->pixels);
        image->pixels = NULL;
        image->width = 0;
        image->height = 0;
    }
}

int png_image_load(const char *path, PngImage *image)
{
    FILE *file = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *rows = NULL;
    unsigned char *data = NULL;
    int ok = 0;
    int y;

    if (!path || !image)
        return 0;

    image->width = 0;
    image->height = 0;
    image->pixels = NULL;

    file = fopen(path, "rb");
    if (!file)
        return 0;

    png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        goto cleanup;

    info = png_create_info_struct(png);
    if (!info)
        goto cleanup;

    if (setjmp(png_jmpbuf(png)))
        goto cleanup;

    png_init_io(png, file);
    png_read_info(png, info);

    {
        png_uint_32 w, h;
        int depth, type, interlace, compression, filter;

        png_get_IHDR(png, info, &w, &h, &depth, &type,
                     &interlace, &compression, &filter);

        if (w == 0 || h == 0 || w > 1024 || h > 1024)
            goto cleanup;

        image->width = (int)w;
        image->height = (int)h;
    }

    if (png_get_bit_depth(png, info) == 16)
        png_set_strip_16(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png, info) < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY ||
        png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    {
        size_t rowbytes = png_get_rowbytes(png, info);
        size_t total = rowbytes * image->height;

        if (rowbytes < (size_t)image->width * 4 ||
            total / rowbytes != (size_t)image->height)
            goto cleanup;

        data = (unsigned char *)malloc(total);
        rows = (png_bytep *)malloc(
            sizeof(png_bytep) * image->height);

        image->pixels = (unsigned int *)malloc(
            (size_t)image->width * image->height *
            sizeof(unsigned int));

        if (!data || !rows || !image->pixels)
            goto cleanup;

        for (y = 0; y < image->height; y++)
            rows[y] = data + y * rowbytes;

        png_read_image(png, rows);
        png_read_end(png, NULL);

        for (y = 0; y < image->height; y++) {
            int x;
            unsigned char *row = rows[y];

            for (x = 0; x < image->width; x++) {
                unsigned char r = row[x * 4 + 0];
                unsigned char g = row[x * 4 + 1];
                unsigned char b = row[x * 4 + 2];
                unsigned char a = row[x * 4 + 3];

                image->pixels[y * image->width + x] =
                    ((unsigned int)a << 24) |
                    ((unsigned int)b << 16) |
                    ((unsigned int)g << 8) |
                    (unsigned int)r;
            }
        }
    }

    ok = 1;

cleanup:
    free(data);
    free(rows);

    if (!ok)
        png_image_free(image);

    if (png || info)
        png_destroy_read_struct(
            png ? &png : NULL,
            info ? &info : NULL,
            NULL);

    fclose(file);
    return ok;
}
