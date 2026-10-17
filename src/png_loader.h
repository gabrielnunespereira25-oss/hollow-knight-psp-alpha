
#ifndef PNG_LOADER_H
#define PNG_LOADER_H

typedef struct {
    int width;
    int height;
    unsigned int *pixels;
} PngImage;

int png_image_load(const char *path, PngImage *image);
void png_image_free(PngImage *image);

#endif

