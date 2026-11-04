#include "image.h"
#include "filter.h"
#include "utils.h"
#include "ascii.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "../external/stb_image_write.h"
#include "glyph_bitmap.h"

#define CHANNEL_VARIATIONS 4

#define BENCHMARK(_label, _call) \
    do { \
        if (benchmark == 1) { \
            struct timespec _step_start, _step_end; \
            clock_gettime(CLOCK_MONOTONIC, &_step_start); \
            _call; \
            clock_gettime(CLOCK_MONOTONIC, &_step_end); \
            fprintf(stderr, "%-28s %7.2lf ms\n", _label, get_time_diff_ms(_step_start, _step_end)); \
        } \
        else { \
            _call; \
        } \
    } while (0)

#define PWD PREFERRED_WIDTH_DIVISOR
#define PHD PREFERRED_HEIGHT_DIVISOR

#define STEPS "12"

typedef enum {
    UNKNOWN,
    HELP,
    DOWNSAMPLE_RES,
    ASCII_RES,
    K_MEANS_TRG_COLORS,
    K_MEANS_ITERATIONS,
    K_MEANS_STRIDE,
    DENOISE_RADIUS,
    ASCII_STRIDE,
    INPUT_PATH,
    OUTPUT_PATH,
    VISUALIZE_STEPS,
    BENCHMARK,
    NAME
} CmdArg;

static void visualise_blobs(const uint8_t *data, int width, int height, const char *path,
                            int visualise_steps) {
    if (!visualise_steps) return;
    FILE *f = fopen(path, "w");
    fprintf(f, "P1\n%d %d\n", width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data[y * width + x] ? fputs("1 ", f) : fputs("0 ", f);
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void add_blobs(const uint8_t *data_new, uint8_t *data_dump, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data_dump[y * width + x] = data_dump[y * width + x] == data_new[y * width + x]
                                           ? data_dump[y * width + x]
                                           : data_new[y * width + x];
        }
    }
}

static void r_ascii(FILE *f, const char **data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fputs(data[y * width + x], f);
        }
        fputc('\n', f);
    }
}

static const char *con_str(const char *str1, const char *str2) {
    unsigned long len1 = strlen(str1);
    unsigned long len2 = strlen(str2);
    char *buffer = malloc(len1 + len2 + 1);
    buffer[0] = '\0';
    strcat(buffer, str1);
    strcat(buffer, str2);
    buffer[len1 + len2] = '\0';
    return buffer;
}

static void visualise_step(Image *image, const char *path, int boolean_visualise) {
    if (boolean_visualise) {
        int err = stbi_write_jpg(path, image->width, image->height, image->channels, image->data, 100);
        if (!err) {
            perror(path);
        }
    }
}

static double get_time_diff_ms(struct timespec t_start, struct timespec t_end) {
    return (double) (t_end.tv_sec - t_start.tv_sec) * 1000 +
           (double) (t_end.tv_nsec - t_start.tv_nsec) / 1e6;
}

static int parse_long(const char *str, long *out) {
    char *end;
    errno = 0;
    long val = strtol(str, &end, 10);

    if (end == str || *end != '\0' || errno == ERANGE) {
        return 1;
    }
    *out = val;
    return 0;
}

static CmdArg parse_argument(const char *arg) {
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) return HELP;
    if (!strcmp(arg, "--resample-res")) return DOWNSAMPLE_RES;
    if (!strcmp(arg, "--ascii-res")) return ASCII_RES;
    if (!strcmp(arg, "--k_means-colors")) return K_MEANS_TRG_COLORS;
    if (!strcmp(arg, "--k_means-iters")) return K_MEANS_ITERATIONS;
    if (!strcmp(arg, "--k_means-stride")) return K_MEANS_STRIDE;
    if (!strcmp(arg, "--denoise-radius")) return DENOISE_RADIUS;
    if (!strcmp(arg, "--ascii-stride")) return ASCII_STRIDE;
    if (!strcmp(arg, "--visualize-steps")) return VISUALIZE_STEPS;
    if (!strcmp(arg, "--benchmark")) return BENCHMARK;
    if (!strcmp(arg, "--input") || !strcmp(arg, "-i")) return INPUT_PATH;
    if (!strcmp(arg, "--output") || !strcmp(arg, "-o")) return OUTPUT_PATH;
    if (!strcmp(arg, "--name")) return NAME;
    return UNKNOWN;
}

static void print_help() {
}

int main(int argc, char *argv[]) {
    const char *input_path = NULL;
    const char *output_path = "./";
    const char *name = "ascii.txt";

    long ascii_w = 200;
    long ascii_h = 100;
    long downsample_w = ascii_w * 1 * 3;
    long downsample_h = ascii_h * 2 * 3;
    long k_means_colors = 5;
    long k_means_iters = 10;
    long k_means_stride = 8;
    long ascii_threshold = 8;

    long *denoise_radius_vals = malloc(4 * sizeof(long));
    denoise_radius_vals[0] = 7; denoise_radius_vals[1] = 1; denoise_radius_vals[2] = 3; denoise_radius_vals[3] = 1;
    int vals_cnt = 4;

    int visualize_steps = 0;
    int benchmark = 0;

    struct timespec total_start, total_end;

    if (argc < 2) {
        print_help();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        CmdArg arg = parse_argument(argv[i]);
        switch (arg) {
            case HELP:
                print_help();
                return 0;

            case INPUT_PATH:
                if (++i >= argc) {
                    fprintf(stderr, "--input requires a path\n");
                    return 1;
                }
                input_path = argv[i];
                break;

            case OUTPUT_PATH:
                if (++i >= argc) {
                    fprintf(stderr, "--output requires a path\n");
                    return 1;
                }
                output_path = argv[i];
                break;

            case DOWNSAMPLE_RES:
                if (i + 2 >= argc) {
                    fprintf(stderr, "--downsample-res requires width height\n");
                    return 1;
                }
                if (parse_long(argv[++i], &downsample_w) || parse_long(argv[++i], &downsample_h) || downsample_w <= 0
                    || downsample_h <= 0) {
                    fprintf(stderr, "invalid dimensions for --downsample-res\n");
                    return 1;
                }
                if (downsample_h > 8640 || downsample_w > 15360) {
                    fprintf(stderr, "crazy?\n");
                    return 1;
                }
                break;

            case ASCII_RES:
                if (i + 2 >= argc) {
                    fprintf(stderr, "--ascii-res requires width height\n");
                    return 1;
                }
                if (parse_long(argv[++i], &ascii_w) || parse_long(argv[++i], &ascii_h)
                    || ascii_w <= 0 || ascii_h <= 0) {
                    fprintf(stderr, "invalid dimensions for --ascii-res\n");
                    return 1;
                }
                if (ascii_w > 2048 || ascii_h > 2048) {
                    fprintf(stderr, "crazy?\n");
                    return 1;
                }
                break;

            case K_MEANS_TRG_COLORS:
                if (++i >= argc) {
                    fprintf(stderr, "--k_means-colors requires a number\n");
                    return 1;
                }
                if (parse_long(argv[i], &k_means_colors) || k_means_colors > CHANNEL_VARIATIONS / sizeof(uint8_t)
                    || k_means_colors < 2) {
                    fprintf(stderr, "invalid value for --k_means-colors: %s."
                            " enter an integer between 2 and 64.\n "
                            "the best value is between 2 and 8\n", argv[i]);
                    return 1;
                }
                break;

            case K_MEANS_ITERATIONS:
                if (++i >= argc) {
                    fprintf(stderr, "--k_means-iters requires a number\n");
                    return 1;
                }
                if (parse_long(argv[i], &k_means_iters) || k_means_iters < 1) {
                    fprintf(stderr, "invalid value for --k_means-iters: %s\n."
                            " the best value is between 10 and 20.", argv[i]);
                    return 1;
                }
                break;

            case K_MEANS_STRIDE:
                if (++i >= argc) {
                    fprintf(stderr, "--k_means-stride requires a number\n");
                    return 1;
                }
                if (parse_long(argv[i], &k_means_stride) || k_means_stride < 1) {
                    fprintf(stderr, "invalid value for --k_means-stride: %s\n."
                            " the best value is between 4 and 16.", argv[i]);
                    return 1;
                }
                break;

            case DENOISE_RADIUS:
                if (++i >= argc) {
                    fprintf(stderr, "--denoise-radius requires a number\n");
                    return 1;
                }
                int cnt = 0;
                for (int j = i; j < argc && argv[j][0] != '-'; j++) {
                    cnt++;
                }
                free(denoise_radius_vals);
                denoise_radius_vals = malloc(cnt * sizeof(long));
                for (int j = 0; j < cnt; j++) {
                    long denoise_radius;
                    if (parse_long(argv[i + j], &denoise_radius) || denoise_radius < 1 || denoise_radius % 2 == 0) {
                        fprintf(stderr, "invalid value for --denoise-radius: %s\n. enter an integer mod 2 != 0.\n"
                                "the best value is between 3 and 9", argv[i]);
                        return 1;
                    }
                    denoise_radius_vals[j] = denoise_radius;
                }
                vals_cnt = cnt;
                i += cnt;
                break;

            case ASCII_STRIDE:
                if (++i >= argc) {
                    fprintf(stderr, "--ascii-stride requires a number\n");
                    return 1;
                }
                if (parse_long(argv[i], &ascii_threshold)) {
                    fprintf(stderr, "invalid value for --ascii-stride: %s\n. the best value is between 3 and 7",
                            argv[i]);
                    return 1;
                }
                break;

            case VISUALIZE_STEPS:
                visualize_steps = 1;
                break;

            case BENCHMARK:
                benchmark = 1;
                break;

            case NAME:
                name = argv[i];
                if (++i >= argc) {
                    fprintf(stderr, "--name requires a non-empty string\n");
                    return 1;
                }
                if (!strcmp(argv[i], "")) {
                    fprintf(stderr, "--name cant be empty\n");
                    return 1;
                }
                break;

            default:
                fprintf(stderr, "unknown argument: %s\n", argv[i]);
                return 1;
        }
    }

    if (benchmark) clock_gettime(CLOCK_MONOTONIC, &total_start);

    Image src_image;
    if (input_path != NULL) {
        image_load(&src_image, input_path);
    } else {
        fprintf(stderr, "--input path required\n");
        return 1;
    }

    Image downsample_image = image_create((int) downsample_w, (int) downsample_h, 3);
    BENCHMARK(con_str("[1/", con_str(STEPS, "] Resample: Area Average")),
              resample_intersection(&src_image, &downsample_image, 1));
    BENCHMARK(con_str("[2/", con_str(STEPS, "] Visualize: Downsampled")),
              visualise_step(&downsample_image, con_str(output_path, "downsampled.jpg"), visualize_steps));

    Palette palette = palette_create(CHANNEL_VARIATIONS);
    BENCHMARK(con_str("[3/", con_str(STEPS, "] Palette: Histogram Sampling")), build_palette(&downsample_image,
                  &palette));

    Image quantized = image_create((int) downsample_w, (int) downsample_h, 3);
    Palette best_colors;
    BENCHMARK(con_str("[4/", con_str(STEPS, "] K-Means: Color Quantization")), best_colors = k_means_quantize(
                  &downsample_image, &quantized, &palette,
                  (int) k_means_colors, (int) k_means_iters));
    BENCHMARK(con_str("[5/", con_str(STEPS, "] Visualize: Quantized")), visualise_step(&quantized,
                  con_str(output_path, "quantised.jpg"),
                  visualize_steps));
    pallette_free(&palette);

    Image denoised = image_create((int) downsample_w, (int) downsample_h, 3);
    Image *curr_denoised;
    BENCHMARK(con_str("[6/", con_str(STEPS, "] Denoise: Morphological Close")),
        for (int i = 0; i < vals_cnt; i++) {
            if (i % 2 == 0) {
                denoise(&quantized, &denoised, (int) denoise_radius_vals[i]);
                curr_denoised = &denoised;
            }
            else {
                denoise(&denoised, &quantized, (int) denoise_radius_vals[i]);
                curr_denoised = &quantized;
            }
        });
    BENCHMARK(con_str("[7/", con_str(STEPS, "] Visualize: Denoised")), visualise_step(curr_denoised,
                  con_str(output_path, "denoised.jpg"), visualize_steps));

    GlyphImage glyph_image = glyph_image_create((int) ascii_w, (int) ascii_h);

    int full_w = glyph_image.width * PWD;
    int full_h = glyph_image.height * PHD;

    uint8_t *data = calloc(curr_denoised->width * curr_denoised->height, sizeof(uint8_t));
    uint8_t *data_resample = calloc(full_w * full_h, sizeof(uint8_t));
    uint8_t *data_edges = calloc(full_w * full_h, sizeof(uint8_t));
    uint8_t *data_dump = calloc(full_w * full_h, sizeof(uint8_t));

    FILE *f = fopen(con_str(output_path, name), "w");

    for (int color_i = 0; color_i < k_means_colors; color_i++) {
        Pixel color = best_colors.data[color_i];

        char num[20];
        snprintf(num, sizeof(num), "%d", color_i + 1);
        fprintf(stderr, con_str(con_str("With color ", num), ":\n"));

        BENCHMARK(con_str("    [8/", con_str(STEPS, "] Blobs: Mask")),
                  to_binary_blobs(curr_denoised, data, color);
                  blobs_bilinear_resample(curr_denoised, data, data_resample, full_w, full_h);
                  blob_find_edges(data_resample, data_edges, full_w, full_h););
        BENCHMARK(con_str("    [9/", con_str(STEPS, "] Accumulate: Binary Frame ")),
                  add_blobs(data_edges, data_dump, full_w, full_h));
        BENCHMARK(con_str("    [10/", con_str(STEPS, "] ASCII: Edges")),
                  to_ascii(&glyph_image, data_edges, (int) ascii_threshold, " "));

        memset(data, 0, curr_denoised->width * curr_denoised->height * sizeof(uint8_t));
        memset(data_resample, 0, full_w * full_h * sizeof(uint8_t));
        memset(data_edges, 0, full_w * full_h * sizeof(uint8_t));
    }
    BENCHMARK(con_str("[11/", con_str(STEPS, "] Output: Binary Dump (PBM)")),
              visualise_blobs(data_dump, full_w, full_h, con_str(output_path,
                  con_str("binary", ".pbm")), visualize_steps));

    BENCHMARK(con_str("[12/", con_str(STEPS, "] Output: ASCII Text")),
              r_ascii(f, glyph_image.glyphs, glyph_image.width, glyph_image.height));

    if (benchmark) {
        clock_gettime(CLOCK_MONOTONIC, &total_end);
        fprintf(stderr, "--------------------------------------------\n");
        fprintf(stderr, "%-28s %7.2lf ms\n", "Total Pipeline Time:", get_time_diff_ms(total_start, total_end));
    }

    free(data);
    free(data_resample);
    free(data_edges);
    free(data_dump);

    return 0;
}
