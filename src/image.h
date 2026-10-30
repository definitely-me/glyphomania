#pragma once
#include <stdint.h>

#define DESIRED_CHANNELS 3

typedef struct {
    int width, height, channels;
    uint8_t *data;
} Image;

typedef struct {
    uint8_t r, g, b;
} Pixel;

typedef struct {
    Pixel *data;
    int channel_variations;
    int size;
} Palette;

Image image_create(int width, int heigh, int channels);

Palette palette_create(int channel_variations);

Pixel image_get_pixel(const Image *image, int x, int y);

Pixel pixel_create(uint8_t r, uint8_t g, uint8_t b);

void pallette_free(Palette *palette);

void image_free(Image *image);

int image_load(Image *image, const char *path);

int image_write(char const *filename, const Image *image);

int pixel_compare(Pixel p1, Pixel p2);

int pixels_compare(Pixel p1, Pixel p2);
