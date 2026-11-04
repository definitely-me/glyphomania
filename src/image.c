#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image.h"
#include "../external/stb_image_write.h"
#include <stdio.h>
#include "image.h"
#include <assert.h>
#include <stdlib.h>
#include "utils.h"


int image_load(Image *image, const char *path) {
    assert(image && path);
    image->data = stbi_load(path, &image->width, &image->height, NULL,
                            DESIRED_CHANNELS);
    image->channels = DESIRED_CHANNELS;
    if (image->data == NULL) {
        fprintf(stderr, "failed to load image '%s'\n", path);
        return 1;
    }
    return 0;
}

void image_free(Image *image) {
    if (image != NULL) stbi_image_free(image->data);
}

void pallette_free(Palette *palette) {
    if (palette != NULL) free(palette->data);
}

Pixel image_get_pixel(const Image *image, int x, int y) {
    assert(image->channels == 3);
    Pixel pixel;
    assert(image);
    pixel.r = image->data[y * image->width * DESIRED_CHANNELS + x * DESIRED_CHANNELS];
    pixel.g = image->data[y * image->width * DESIRED_CHANNELS + x * DESIRED_CHANNELS + 1];
    pixel.b = image->data[y * image->width * DESIRED_CHANNELS + x * DESIRED_CHANNELS + 2];
    return pixel;
}

// int image_write(const char *path, const Image *image) {
//     assert(path && image);
//     return stbi_write_jpg(path, image->width, image->height, image->channels, image->data, 100);
// }

Image image_create(int width, int height, int channels) {
    assert(width > 0 && height > 0);
    Image image;
    image.width = width;
    image.height = height;
    image.channels = channels;
    image.data = calloc(width * height * channels, sizeof(uint8_t));
    return image;
}

Palette palette_create(int channel_variations) {
    Palette palette;
    palette.channel_variations = channel_variations;
    palette.size = ipow(channel_variations, 3);
    palette.data = malloc(sizeof(Pixel) * ipow(channel_variations, 3));
    return palette;
}

int pixel_compare(Pixel p1, Pixel p2) {
    int cnt = 0;
    cnt += (p1.r == p2.r);
    cnt += (p1.g == p2.g);
    cnt += (p1.b == p2.b);

    if (cnt == 3) return 1;
    return 0;
}

int pixels_compare(Pixel p1, Pixel p2) {
    if (p1.r != p2.r || p1.g != p2.g || p1.b != p2.b) return 0;
    return 1;
}

Pixel pixel_create(uint8_t r, uint8_t g, uint8_t b) {
    Pixel p;
    p.r = r;
    p.g = g;
    p.b = b;
    return p;
}
