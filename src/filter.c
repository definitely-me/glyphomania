#include "../external/stb_image.h"
#include <stdio.h>
#include "image.h"
#include <assert.h>
#include <math.h>
#include "filter.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include <limits.h>


static int color_distance_sq(Pixel p1, Pixel p2) {
    int dr = (int) p1.r - (int) p2.r;
    int dg = (int) p1.g - (int) p2.g;
    int db = (int) p1.b - (int) p2.b;
    return dr * dr + dg * dg + db * db;
}

static int color_power(Pixel p) {
    return p.r * p.r + p.g * p.g + p.b * p.b;
}

static int get_pixel_index(Pixel pixel, int channel_variations) {
    int step = 256 / channel_variations;
    int qr = pixel.r / step;
    if (qr >= channel_variations) qr = channel_variations - 1;
    int qg = pixel.g / step;
    if (qg >= channel_variations) qg = channel_variations - 1;
    int qb = pixel.b / step;
    if (qb >= channel_variations) qb = channel_variations - 1;

    int index = qr * ipow(channel_variations, 2) + qg * channel_variations + qb;
    return index;
}

static Pixel get_pixel(int index, int channel_variations) {
    int step = 256 / channel_variations;

    int qb = index % channel_variations;
    int qg = (index / channel_variations) % channel_variations;
    int qr = index / ipow(channel_variations, 2);

    Pixel pixel;
    pixel.r = qr * step + (step / 2);
    pixel.g = qg * step + (step / 2);
    pixel.b = qb * step + (step / 2);
    return pixel;
}


void build_palette(const Image *src_image, Palette *palette) {
    int channel_variations = palette->channel_variations;
    int width = src_image->width;
    int height = src_image->height;
    int total_bins = palette->size;
    ColorEntry *candidates = calloc(total_bins, sizeof(ColorEntry));
    for (int src_x = 0; src_x < width; src_x++) {
        for (int src_y = 0; src_y < height; src_y++) {
            Pixel pixel = image_get_pixel(src_image, src_x, src_y);
            int candidate_index = get_pixel_index(pixel, channel_variations);
            candidates[candidate_index].count++;
            candidates[candidate_index].color_index = candidate_index;
        }
    }

    int candidates_passed = 0;

    for (int i = 0; i < total_bins; i++) {
        ColorEntry candidate = candidates[i];
        if (candidate.count > PALETTE_THRESHOLD) {
            candidates[candidates_passed] = candidate;
            candidates_passed++;
        }
    }

    for (int i = 0; i < palette->size; i++) {
        // the darkest
        if (i == 0) {
            int min_power = INT_MAX;
            Pixel min_power_p;
            for (int c = 0; c < candidates_passed; c++) {
                Pixel curr_p = get_pixel(candidates[c].color_index, channel_variations);
                int power = color_power(curr_p);
                if (min_power > power) {
                    min_power = power;
                    min_power_p = curr_p;
                }
            }
            palette->data[i] = min_power_p;
        }

        //the lightest
        else if (i == 1) {
            int max_power = INT_MIN;
            Pixel max_power_p;
            for (int c = 0; c < candidates_passed; c++) {
                Pixel curr_p = get_pixel(candidates[c].color_index, channel_variations);
                int power = color_power(curr_p);
                if (max_power < power) {
                    max_power = power;
                    max_power_p = curr_p;
                }
            }
            palette->data[i] = max_power_p;
        } else {
            int max_distance = INT_MIN;
            Pixel best_p;

            for (int c = 0; c < candidates_passed; c++) {
                Pixel curr_p = get_pixel(candidates[c].color_index, channel_variations);

                int min_dist_to_palette = INT_MAX;
                for (int prev = 0; prev < i; prev++) {
                    int dist = color_distance_sq(curr_p, palette->data[prev]);
                    if (dist < min_dist_to_palette) min_dist_to_palette = dist;
                }

                if (min_dist_to_palette > max_distance) {
                    max_distance = min_dist_to_palette;
                    best_p = curr_p;
                }
            }

            palette->data[i] = best_p;
        }
    }

    free(candidates);
}


static int find_nearest_color(const Pixel src_p, const Palette *palette, int target_colors) {
    int min_distance = INT_MAX;
    int best_pixel = 0;

    for (int i = 0; i < target_colors; i++) {
        Pixel palette_p = palette->data[i];
        int cd_sq = color_distance_sq(src_p, palette_p);
        if (cd_sq < min_distance) {
            min_distance = cd_sq;
            best_pixel = i;
        }
    }
    return best_pixel;
}

static int compare_by_frequency_desc(const void *a, const void *b) {
    const PixelEntry *ca = a;
    const PixelEntry *cb = b;

    if (cb->count > ca->count) return 1;
    if (cb->count < ca->count) return -1;
    return 0;
}

static void sort_pixel_entries(PixelEntry *p, int target_colors) {
    qsort(p, target_colors, sizeof(PixelEntry), compare_by_frequency_desc);
}

Palette k_means_quantize(const Image *src_image, Image *trg_image, const Palette *palette, int target_colors,
                         int iterations) {
    int width = trg_image->width;
    int height = trg_image->height;

    Pixel *best_colors_data = malloc(target_colors * sizeof(Pixel));
    for (int i = 0; i < target_colors; i++) {
        best_colors_data[i] = palette->data[i];
    }

    Palette curr_palette;
    curr_palette.data = best_colors_data;
    curr_palette.size = target_colors;
    curr_palette.channel_variations = palette->channel_variations;

    PixelEntry *means = malloc(target_colors * sizeof(PixelEntry));

    while (iterations > 0) {
        memset(means, 0, target_colors * sizeof(PixelEntry));

        for (int src_y = 0; src_y < height; src_y++) {
            for (int src_x = 0; src_x < width; src_x += KMEANS_STRIDE) {
                Pixel src_p = image_get_pixel(src_image, src_x, src_y);
                int best_pixel = find_nearest_color(src_p, &curr_palette, target_colors);

                means[best_pixel].r_accum += src_p.r;
                means[best_pixel].g_accum += src_p.g;
                means[best_pixel].b_accum += src_p.b;
                means[best_pixel].count++;
            }
        }

        for (int i = 0; i < target_colors; i++) {
            if (means[i].count > 0) {
                best_colors_data[i].r = (uint8_t) (means[i].r_accum / means[i].count);
                best_colors_data[i].g = (uint8_t) (means[i].g_accum / means[i].count);
                best_colors_data[i].b = (uint8_t) (means[i].b_accum / means[i].count);
            }
            if (iterations == 1) {
                means[i].r_accum = best_colors_data[i].r;
                means[i].g_accum = best_colors_data[i].g;
                means[i].b_accum = best_colors_data[i].b;
            }
        }
        iterations--;
    }

    for (int src_y = 0; src_y < height; src_y++) {
        for (int src_x = 0; src_x < width; src_x++) {
            Pixel p = image_get_pixel(src_image, src_x, src_y);
            int nearest = find_nearest_color(p, &curr_palette, target_colors);
            int ptr = (src_y * width + src_x) * trg_image->channels;
            trg_image->data[ptr] = best_colors_data[nearest].r;
            trg_image->data[ptr + 1] = best_colors_data[nearest].g;
            trg_image->data[ptr + 2] = best_colors_data[nearest].b;
        }
    }

    sort_pixel_entries(means, target_colors);

    for (int i = 0; i < target_colors; i++) {
        curr_palette.data[i] = pixel_create(means[i].r_accum, means[i].g_accum, means[i].b_accum);
    }

    free(means);
    return curr_palette;
}

void downsample(const Image *src_image, Image *trg_image, int factor) {
    assert(src_image->width > 0 && src_image->height > 0 && src_image != NULL && trg_image != NULL
        && src_image->channels == 3 && trg_image->channels == 3);

    int width = trg_image->width;
    int height = trg_image->height;

    int src_width = src_image->width;
    int src_height = src_image->height;
    double ratio_width = (double) src_width / width;
    double ratio_height = (double) src_height / height;

    for (int trg_y = 0; trg_y < height; trg_y++) {
        for (int trg_x = 0; trg_x < width; trg_x++) {
            double trg_start_x = trg_x * ratio_width;
            double trg_end_x = (trg_x + 1) * ratio_width;
            double trg_start_y = trg_y * ratio_height;
            double trg_end_y = (trg_y + 1) * ratio_height;
            double trgS = ratio_width * ratio_height;

            double r = 0, g = 0, b = 0;

            for (int src_y = (int) trg_start_y; src_y < trg_end_y; src_y++) {
                for (int src_x = (int) trg_start_x; src_x < trg_end_x; src_x++) {
                    double part_x = intersection(trg_start_x, src_x, trg_end_x, src_x + 1);
                    double part_y = intersection(trg_start_y, src_y, trg_end_y, src_y + 1);
                    double part_x_y = part_x * part_y;

                    Pixel pixel = image_get_pixel(src_image, src_x, src_y);
                    r += pixel.r * part_x_y;
                    g += pixel.g * part_x_y;
                    b += pixel.b * part_x_y;
                }
            }
            int ptr = (trg_y * width + trg_x) * trg_image->channels;
            trg_image->data[ptr] = (u_int8_t) (r / trgS);
            trg_image->data[ptr + 1] = (u_int8_t) (g / trgS);
            trg_image->data[ptr + 2] = (u_int8_t) (b / trgS);
        }
    }
}

void denoise(const Image *src_image, Image *trg_image, int side_size) {
    assert(side_size % 2 != 0); //there is an exact center

    int width = trg_image->width;
    int height = trg_image->height;
    int len = ipow(side_size, 2);

    CounterEntry map[len];
    memset(map, 0, len * sizeof(map[0]));

    for (int trg_y = 0; trg_y < height; trg_y++) {
        for (int trg_x = 0; trg_x < width; trg_x++) {
            int top = imax(trg_y - side_size / 2, 0);
            int bot = imin(trg_y + side_size / 2 + 1, height);

            int left = imax(trg_x - side_size / 2, 0);
            int right = imin(trg_x + side_size / 2 + 1, width);

            int cnt = 0;

            for (int src_y = top; src_y < bot; src_y++) {
                for (int src_x = left; src_x < right; src_x++) {
                    Pixel current_pixel = image_get_pixel(src_image, src_x, src_y);

                    int found = 0;

                    for (int i = 0; i < cnt; i++) {
                        if (pixel_compare(current_pixel, map[i].pixel) == 0) {
                            map[i].count++;
                            found = 1;
                        }
                    }
                    if (found == 0) {
                        map[cnt].pixel = current_pixel;
                        map[cnt].count++;
                        cnt++;
                    }
                }
            }

            int max_count = INT_MIN;
            Pixel pixel;
            for (int i = 0; i < len; i++) {
                if (max_count < map[i].count) {
                    max_count = map[i].count;
                    pixel = map[i].pixel;
                }
            }
            trg_image->data[(trg_y * width + trg_x) * 3] = pixel.r;
            trg_image->data[(trg_y * width + trg_x) * 3 + 1] = pixel.g;
            trg_image->data[(trg_y * width + trg_x) * 3 + 2] = pixel.b;
            memset(map, 0, len * sizeof(map[0]));
        }
    }
}
