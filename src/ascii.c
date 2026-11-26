#include "ascii.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#define PHD PREFERRED_HEIGHT_DIVISOR
#define PWD PREFERRED_WIDTH_DIVISOR

static int min_left = 1000;
static int min_right = 1000;
static int min_wide = 1000;


static const GlyphTile* GLYPHS;
static int GLYPH_WIDE_OFFSET;
static int GLYPH_TILE_COUNT;

void parse_rodata(GlyphTile* glyphs, int gwo, int gtc) {
    GLYPHS = glyphs;
    GLYPH_WIDE_OFFSET = gwo;
    GLYPH_TILE_COUNT = gtc;
}

GlyphImage glyph_image_create(int width, int height) {
    GlyphImage image;
    image.width = width;
    image.height = height;
    image.glyphs = calloc(width * height, sizeof(const char*));
    return image;
}

void blob_find_edges(const uint8_t* src, uint8_t* trg, int width, int height) {
    int dx[4] = {-1, 0, 1, 0};
    int dy[4] = {0, 1, 0, -1};
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int curr_index = y * width + x;
            int curr = src[curr_index];

            if (curr == 1) {
                int found = 0;

                for (int i = 0; i < 4; i++) {
                    int y_dy = (y + dy[i]) * width;
                    int x_dx = x + dx[i];
                    if (y_dy + x_dx >= 0 && y_dy + x_dx < height * width) {
                        if (src[y_dy + x_dx] == 0) {
                            found = 1;
                            break;
                        }
                    }
                }
                if (found) {
                    trg[curr_index] = 1;
                }
            }
        }
    }
}

void to_binary_blobs(const Image* src, uint8_t* data, Pixel blob_color) {
    for (int src_y = 0; src_y < src->height; src_y++) {
        for (int src_x = 0; src_x < src->width; src_x++) {
            if (pixel_compare(image_get_pixel(src, src_x, src_y), blob_color)) {
                data[src_y * src->width + src_x] = 1;
            }
        }
    }
}

void glyph_image_free(GlyphImage* trg) {
    free(trg->glyphs);
}

void blobs_bilinear_resample(const Image* src, const uint8_t* src_data, uint8_t* trg_data, int trg_width,
                             int trg_height) {
    int width = src->width;
    int height = src->height;

    if (width == trg_width && height == trg_height) {
        memcpy(trg_data, src_data, trg_width * trg_height);
        return;
    }

    float ratio_x = trg_width / (float)width;
    float ratio_y = trg_height / (float)height;

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

            int x = (int)src_x;
            int y = (int)src_y;

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


static void matrix2d_get_slice(const uint8_t* src, uint8_t* slice, int src_width, int src_height, int slice_x,
                               int slice_y, int width, int height) {
    for (int y = 0; y < height; y++) {
        const uint8_t* src_row_start = &src[(slice_y + y) * src_width + slice_x];
        uint8_t* dst_row_start = &slice[y * width];
        memcpy(dst_row_start, src_row_start, width);
    }
}

static int test_slice(const uint8_t* src, int threshold, int width, int height, int value) {
    int count = 0;
    if (threshold <= 0) return 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (src[y * width + x] == value) {
                count++;
                if (count >= threshold) return 1;
            }
        }
    }
    return 0;
}

static int dist_matrix2d_compare(const uint8_t* m1, const uint8_t* m2, int size) {
    int diff = 0;
    for (int i = 0; i < size; i++) {
        diff += abs((int)m1[i] - (int)m2[i]);
    }
    return diff;
}

static BestGlyph find_best_narrow(uint8_t* matrix, int width, int height) {
    BestGlyph best = {INT_MAX, NULL};

    for (int i = 0; i < GLYPH_WIDE_OFFSET; i++) {
        int diff = dist_matrix2d_compare(matrix, GLYPHS[i].bitmap, width * height);
        if (diff < best.min_diff) {
            best.min_diff = diff;
            best.glyph = GLYPHS[i].glyph;
        }
    }

    return best;
}

static BestGlyph find_best_wide(uint8_t** matrix_left_right, int width, int height) {
    BestGlyph best = {INT_MAX, NULL};

    for (int i = GLYPH_WIDE_OFFSET; i + 1 < GLYPH_TILE_COUNT; i += 2) {
        int diff = 0;
        for (int j = 0; j < 2; j++) {
            uint8_t* matrix = matrix_left_right[j];
            diff += dist_matrix2d_compare(matrix, GLYPHS[i + j].bitmap, width * height);
        }
        if (diff < best.min_diff) {
            best.min_diff = diff;
            best.glyph = GLYPHS[i].glyph;
        }
    }

    return best;
}

static int decide_wide(BestGlyphs best, int wide_bias) {
    if (best.left.min_diff < min_left && best.left.min_diff > 0) min_left = best.left.min_diff;
    if (best.right.min_diff < min_right && best.right.min_diff > 0) min_right = best.right.min_diff;
    if (best.wide.min_diff < min_wide && best.wide.min_diff > 0) min_wide = best.wide.min_diff;
    if (best.left.min_diff < 0) return 0;
    if (best.right.min_diff < 0) return 0;
    if (best.wide.min_diff - (best.left.min_diff + best.right.min_diff) < wide_bias) return 1;

    return 0;
}

static int prepare_cell(const uint8_t* data, int src_w, int src_h, int trg_x, int trg_y,
                        int threshold, int ink_bias, uint8_t* out_dist) {
    uint8_t slice[PWD * PHD] = {0};
    matrix2d_get_slice(data, slice, src_w, src_h, trg_x * PWD, trg_y * PHD, PWD, PHD);

    if (!test_slice(slice, threshold, PWD, PHD, 1)) {
        return 0;
    }

    for (int y = 0; y < PHD; y++) {
        for (int x = 0; x < PWD; x++) {
            out_dist[y * PWD + x] = (uint8_t)clamp_to_zero(bfs_nearest(slice, PWD, PHD, x, y, 1) + ink_bias);
        }
    }
    return 1;
}

static int is_cell_free(const char* glyph, const char* empty_glyph) {
    return (glyph == NULL || glyph == empty_glyph) && (glyph == NULL || glyph[0] != NOT_EMPTY_WIDE[0]);
}

AsciiCtx to_ascii(GlyphImage* trg, const uint8_t* data, int threshold, const char* empty_glyph, int ink_bias,
                  int use_wide, int wide_bias) {
    AsciiCtx ctx = {.wide_cnt = 0, .narrow_cnt = 0};

    int src_w = trg->width * PWD;
    int src_h = trg->height * PHD;
    int height = trg->height;
    int width = trg->width;

    for (int trg_y = 0; trg_y < height; trg_y++) {
        int trg_x = 0;

        while (trg_x < width) {
            int idx1 = trg_y * width + trg_x;

            if (!is_cell_free(trg->glyphs[idx1], empty_glyph)) {
                trg_x++;
                continue;
            }

            uint8_t bitmap1[PWD * PHD];
            if (!prepare_cell(data, src_w, src_h, trg_x, trg_y, threshold, ink_bias, bitmap1)) {
                trg->glyphs[idx1] = empty_glyph;
                trg_x++;
                continue;
            }

            int idx2 = idx1 + 1;
            uint8_t bitmap2[PWD * PHD];
            int can_try_wide = use_wide && (trg_x + 1 < width) && is_cell_free(trg->glyphs[idx2], empty_glyph)
                && prepare_cell(data, src_w, src_h, trg_x + 1, trg_y, threshold, ink_bias, bitmap2);

            if (!can_try_wide) {
                trg->glyphs[idx1] = find_best_narrow(bitmap1, PWD, PHD).glyph;
                ctx.narrow_cnt++;
                trg_x++;
                continue;
            }

            uint8_t* pair[2] = {bitmap1, bitmap2};
            BestGlyphs best = {
                .left = find_best_narrow(bitmap1, PWD, PHD),
                .right = find_best_narrow(bitmap2, PWD, PHD),
                .wide = find_best_wide(pair, PWD, PHD),
            };

            if (decide_wide(best, wide_bias)) {
                trg->glyphs[idx1] = best.wide.glyph;
                trg->glyphs[idx2] = NOT_EMPTY_WIDE;
                ctx.wide_cnt++;
            }
            else {
                trg->glyphs[idx1] = best.left.glyph;
                trg->glyphs[idx2] = best.right.glyph;
                ctx.narrow_cnt += 2;
            }
            trg_x += 2;
        }
    }

    return ctx;
}
