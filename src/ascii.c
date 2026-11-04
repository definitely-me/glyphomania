#include "ascii.h"
#include "glyph_bitmap.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#define PHD PREFERRED_HEIGHT_DIVISOR
#define PWD PREFERRED_WIDTH_DIVISOR

GlyphImage glyph_image_create(int width, int height) {
    GlyphImage image;
    image.width = width;
    image.height = height;
    image.glyphs = calloc(width * height, sizeof(const char *));
    return image;
}

void blob_find_edges(const uint8_t *src, uint8_t *trg, int width, int height) {
    int dx[4] = {-1, 0, 1, 0};
    int dy[4] = {0, 1, 0, -1};
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int curr_index = y * width + x;
            int curr = src[curr_index];

            if (curr == 1) {
                int found = 0;

                for (int i = 0; i < 4; i++) {
                    if (src[(y + dy[i]) * width + x + dx[i]] == 0) {
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    trg[curr_index] = 1;
                }
            }
        }
    }
}

void to_binary_blobs(const Image *src, uint8_t *data, Pixel blob_color) {
    for (int src_y = 0; src_y < src->height; src_y++) {
        for (int src_x = 0; src_x < src->width; src_x++) {
            if (pixel_compare(image_get_pixel(src, src_x, src_y), blob_color)) {
                data[src_y * src->width + src_x] = 1;
            }
        }
    }
}

void blobs_bilinear_resample(const Image *src, const uint8_t *src_data, uint8_t *trg_data, int trg_width,
                             int trg_height) {
    int width = src->width;
    int height = src->height;

    float ratio_x = trg_width / (float) width;
    float ratio_y = trg_height / (float) height;

    int dx[4] = {0, 0, 1, 1};
    int dy[4] = {0, 1, 0, 1};

    for (int trg_y = 0; trg_y < trg_height; trg_y++) {
        for (int trg_x = 0; trg_x < trg_width; trg_x++) {
            float bucket_0 = 0;
            float bucket_1 = 0;

            float src_x = trg_x / ratio_x;
            float src_y = trg_y / ratio_y;

            float factor_x = src_x - floor_f(src_x);
            float factor_y = src_y - floor_f(src_y);

            float factors[8] = {
                1 - factor_x, 1 - factor_y, 1 - factor_x, factor_y, factor_x, 1 - factor_y, factor_x,
                factor_y
            };

            int x = (int) src_x;
            int y = (int) src_y;

            for (int i = 0; i < 4; i++) {
                int index = (y + dy[i]) * width + x + dx[i];
                if (index >= 0 && index < width * height && x + dx[i] >= 0 && x + dx[i] < width && y + dy[i] >= 0
                    && y + dy[i] < height) {
                    uint8_t curr = src_data[index];
                    if (curr) bucket_1 += factors[i * 2] * factors[i * 2 + 1];
                    else bucket_0 += factors[i * 2] * factors[i * 2 + 1];
                }
            }

            if (bucket_0 < bucket_1) trg_data[trg_y * trg_width + trg_x] = 1;
            else trg_data[trg_y * trg_width + trg_x] = 0;
        }
    }
}


static void matrix2d_get_slice(const uint8_t *src, uint8_t *slice, int src_width, int src_height, int slice_x,
                               int slice_y, int width, int height) {
    for (int y = 0; y < height; y++) {
        const uint8_t *src_row_start = &src[(slice_y + y) * src_width + slice_x];
        uint8_t *dst_row_start = &slice[y * width];
        memcpy(dst_row_start, src_row_start, width);
    }
}

static int test_slice(const uint8_t *src, int threshold, int width, int height, int value) {
    int count = 0;
    if (threshold <= 0) return 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (src[y * width + x] == value) {
                count++;
                if (count >= threshold) return 0;
            }
        }
    }
    return 1;
}

static int dist_matrix2d_compare(const uint8_t *m1, const uint8_t *m2, int size) {
    int diff = 0;
    for (int i = 0; i < size; i++) {
        diff += abs((int) m1[i] - (int) m2[i]);
    }
    return diff;
}


void to_ascii(GlyphImage *trg, const uint8_t *data, int threshold, const char *empty_glyph) {
    int src_w = trg->width * PWD;
    int src_h = trg->height * PHD;
    int height = trg->height;
    int width = trg->width;

    uint8_t *slice = malloc(PWD * PHD * sizeof(uint8_t));
    for (int trg_y = 0; trg_y < height; trg_y++) {
        for (int trg_x = 0; trg_x < width; trg_x++) {
            if (trg->glyphs[trg_y * width + trg_x] != NULL && trg->glyphs[trg_y * width + trg_x] != empty_glyph)
                continue;

            memset(slice, 0, PWD * PHD * sizeof(uint8_t));

            matrix2d_get_slice(data, slice, src_w, src_h, trg_x * PWD, trg_y * PHD,
                               PWD, PHD);

            const char *glyph = empty_glyph;

            if (test_slice(slice, threshold, PWD, PHD, 1) == 0) {
                int min_diff = INT_MAX;
                const char *min_diff_glyph = empty_glyph;

                uint8_t dist_matrix2d[PWD * PHD] = {0};

                for (int y = 0; y < PHD; y++) {
                    for (int x = 0; x < PWD; x++) {
                        dist_matrix2d[y * PWD + x] = (uint8_t) bfs_nearest(slice, PWD, PHD,
                                                                           x, y, 1);
                    }
                }
                for (int i = 0; i < GLYPH_WIDE_OFFSET; i++) {
                    int diff = dist_matrix2d_compare(dist_matrix2d, GLYPHS[i].bitmap, PWD * PHD);
                    if (diff < min_diff) {
                        min_diff = diff;
                        min_diff_glyph = GLYPHS[i].glyph;
                    }
                }
                glyph = min_diff_glyph;
            }
            trg->glyphs[trg_y * width + trg_x] = glyph;
        }
    }
    free(slice);
}
