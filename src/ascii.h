#pragma once

#include "image.h"

typedef struct {
    int width;
    int height;
    const char **glyphs;
} GlyphImage;

GlyphImage glyph_image_create(int width, int height);

void to_binary_blobs(const Image *src, uint8_t *data, Pixel blob_color);

void to_ascii(GlyphImage *trg, const uint8_t *data, int threshold, const char *empty_glyph);

void glyph_image_free(GlyphImage *trg);

void blobs_bilinear_resample(const Image *src, const uint8_t *src_data, uint8_t *trg_data, int trg_width,
                             int trg_height);

void blob_find_edges(const uint8_t *src, uint8_t *trg, int width, int height);
