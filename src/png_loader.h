
#ifndef PNG_LOADER_H
#define PNG_LOADER_H

typedef struct {
    int width;
    int height;
    unsigned int *pixels;
} PngImage;

int psp_png_load(const char *path, PngImage *image);
void psp_png_free(PngImage *image);

#endif
