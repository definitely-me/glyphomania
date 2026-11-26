#pragma once

#include "image.h"

#define NOT_EMPTY_WIDE ""
#define PREFERRED_WIDTH_DIVISOR 8
#define PREFERRED_HEIGHT_DIVISOR 16

typedef struct __attribute__((__packed__)) {
    uint8_t is_wide;
    uint32_t glyph_codepoint;
    uint8_t bitmap[PREFERRED_WIDTH_DIVISOR * PREFERRED_HEIGHT_DIVISOR];
} Record;

typedef struct __attribute__((__packed__)) {
    int length;
    Record records[];
} Header;

typedef struct {
    const char* glyph;
    const uint8_t* bitmap;
} GlyphTile;

typedef struct {
    int width;
    int height;
    const char** glyphs;
} GlyphImage;

typedef struct {
    int min_diff;
    const char* glyph;
} BestGlyph;

typedef struct {
    BestGlyph left;
    BestGlyph right;
    BestGlyph wide;
} BestGlyphs;

typedef struct {
    int wide_cnt;
    int narrow_cnt;
} AsciiCtx;

void parse_rodata(GlyphTile* glyphs, int gwo, int gtc);

GlyphImage glyph_image_create(int width, int height);

void to_binary_blobs(const Image* src, uint8_t* data, Pixel blob_color);

AsciiCtx to_ascii(GlyphImage* trg, const uint8_t* data, int threshold, const char* empty_glyph, int ink_bias,
                  int use_wide, int wide_bias);

void glyph_image_free(GlyphImage* trg);

void blobs_bilinear_resample(const Image* src, const uint8_t* src_data, uint8_t* trg_data, int trg_width,
                             int trg_height);

void blob_find_edges(const uint8_t* src, uint8_t* trg, int width, int height);
