#pragma once
#include "image.h"

#define PALETTE_THRESHOLD 8
#define KMEANS_STRIDE 8

typedef struct {
    int color_index;
    int count;
} ColorEntry;

typedef struct {
    Pixel pixel;
    int count;
} CounterEntry;

typedef struct {
    long r_accum;
    long g_accum;
    long b_accum;
    long count;
} PixelEntry;

static int cmp_entries(const void *a, const void *b);

void downsample(const Image *src_image, Image *trg_image, int factor);

//void to_oklab_L(const Image *src_image, Image *trg_image);

void kuwahara(const Image *src_image, Image *trg_image, int radius);

void build_palette(const Image *src_image, Palette *palette);

Palette k_means_quantize(const Image *src_image, Image *trg_image, const Palette *palette, int target_colors,
                         int iterations);

void denoise(const Image *src_image, Image *trg_image, int side_size);
