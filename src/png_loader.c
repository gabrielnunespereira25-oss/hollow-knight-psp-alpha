
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "png_loader.h"

int psp_png_load(const char *path, PngImage *image)
{
    FILE *file;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *rows;
    png_uint_32 width, height;
    int bit_depth, color_type;
    int x, y;

    if (path == NULL || image == NULL)
        return 0;

    image->width = 0;
    image->height = 0;
    image->pixels = NULL;

    file = fopen(path, "rb");
    if (file == NULL)
        return 0;

    {
        unsigned char signature[8];

        if (fread(signature, 1, 8, file) != 8 ||
            png_sig_cmp(signature, 0, 8)) {
            fclose(file);
            return 0;
        }
    }

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
        fclose(file);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(file);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        free(image->pixels);
        image->pixels = NULL;
        image->width = 0;
        image->height = 0;

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    png_init_io(png_ptr, file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    if (width == 0 || height == 0 ||
        width > 1024 || height > 1024) {
        png_error(png_ptr, "Invalid image dimensions");
    }

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY &&
        bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    if (!(color_type & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png_ptr, info_ptr);

    image->pixels = (unsigned int *)malloc(
        (size_t)width * height * sizeof(unsigned int));

    if (image->pixels == NULL)
        png_error(png_ptr, "Out of memory");

    rows = (png_bytep *)malloc(
        sizeof(png_bytep) * height);

    if (rows == NULL)
        png_error(png_ptr, "Out of memory");

    for (y = 0; y < (int)height; y++) {
        rows[y] = (png_bytep)malloc(
            png_get_rowbytes(png_ptr, info_ptr));

        if (rows[y] == NULL) {
            while (y > 0)
                free(rows[--y]);

            free(rows);
            png_error(png_ptr, "Out of memory");
        }
    }

    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, NULL);

    for (y = 0; y < (int)height; y++) {
        for (x = 0; x < (int)width; x++) {
            png_bytep p = &rows[y][x * 4];

            image->pixels[y * width + x] =
                ((unsigned int)p[3] << 24) |
                ((unsigned int)p[2] << 16) |
                ((unsigned int)p[1] << 8) |
                (unsigned int)p[0];
        }

        free(rows[y]);
    }

    free(rows);

    image->width = (int)width;
    image->height = (int)height;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(file);

    return 1;
}

void psp_png_free(PngImage *image)
{
    if (image == NULL)
        return;

    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}
