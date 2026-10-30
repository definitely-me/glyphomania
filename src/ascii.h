#pragma once

#include "image.h"

typedef struct {
    int width;
    int height;
    const char **glyphs;
} GlyphImage;

GlyphImage glyph_image_create(int width, int height);

void to_binary_blobs(const Image *src, uint8_t *data, Pixel blob_color);

void to_ascii(const Image *src, GlyphImage *trg, const Pixel *colors_data, int target_colors,
              int threshold, const char *empty_glyph);

void glyph_image_free(GlyphImage *trg);
