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

void to_binary_blobs(const Image *src, uint8_t *data, Pixel blob_color) {
    int width = src->width;
    int height = src->height;
    int dx[4] = {-1, 0, 1, 0};
    int dy[4] = {0, 1, 0, -1};
    for (int src_y = 0; src_y < height; src_y++) {
        for (int src_x = 0; src_x < width; src_x++) {
            if (pixel_compare(image_get_pixel(src, src_x, src_y), blob_color) == 0) {
                int found = 0;
                for (int i = 0; i < 4; i++) {
                    int x = src_x + dx[i];
                    int y = src_y + dy[i];
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        if (pixel_compare(image_get_pixel(src, x, y), blob_color) != 0) {
                            found = 1;
                        }
                    }
                    if (found == 1) {
                        data[src_y * width + src_x] = 1;
                        break;
                    }
                    data[src_y * width + src_x] = 0;
                }
            } else {
                data[src_y * width + src_x] = 0;
            }
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


void to_ascii(const Image *src, GlyphImage *trg, const Pixel *colors_data, int target_colors,
              int threshold, const char *empty_glyph) {
    int width = trg->width;
    int height = trg->height;
    printf("%d", round_up(src->width, PWD));
    assert(width == round_up(src->width, PWD));
    assert(height == round_up(src->height, PHD));
    uint8_t *slice = malloc(PWD * PHD * sizeof(uint8_t));

    for (int color_i = 0; color_i < target_colors; color_i++) {
        if (color_i == 1) continue;

        uint8_t *data = malloc(src->height * src->width * sizeof(uint8_t));

        to_binary_blobs(src, data, colors_data[color_i]);

        for (int trg_y = 0; trg_y < height; trg_y++) {
            for (int trg_x = 0; trg_x < width; trg_x++) {
                if (trg->glyphs[trg_y * width + trg_x] != NULL) continue;

                memset(slice, 0, PWD * PHD * sizeof(uint8_t));

                matrix2d_get_slice(data, slice, src->width, src->height, trg_x * PWD, trg_y * PHD, PWD, PHD);

                const char *glyph = NULL;

                if (test_slice(slice, threshold, PWD, PHD, 1) == 0) {
                    int min_diff = INT_MAX;
                    const char *min_diff_glyph = " ";

                    uint8_t dist_matrix2d[PWD * PHD] = {0};

                    for (int y = 0; y < PHD; y++) {
                        for (int x = 0; x < PWD; x++) {
                            dist_matrix2d[y * PWD + x] = (uint8_t) bfs_nearest(slice, PWD, PHD, x, y, 1);
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
        free(data);
    }
    free(slice);
}
