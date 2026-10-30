#include "image.h"
#include "filter.h"
#include "utils.h"
#include "ascii.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEBUG_WIDTH 3200
#define DEBUG_HEIGHT 3200
#define CHANNEL_VARIATIONS 4
#define TARGET_COLORS 2

static double get_time_diff_ms(struct timespec t_start, struct timespec t_end) {
    return (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
           (t_end.tv_nsec - t_start.tv_nsec) / 1000000.0;
}

int main(void) {
    struct timespec total_start, total_end;
    struct timespec step_start, step_end;

    clock_gettime(CLOCK_MONOTONIC, &total_start);

    /* 1. Загрузка исходника */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    Image src_image;
    image_load(&src_image, "eww.jpeg");
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[IO]   image_load:         %7.2f мс (%dx%d)\n",
           get_time_diff_ms(step_start, step_end),
           src_image.width, src_image.height);
    /* 2. Даунскейл / Апскейл */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    Image downsample_image = image_create(DEBUG_WIDTH, DEBUG_HEIGHT, 3);
    downsample(&src_image, &downsample_image, 1);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[PROC] downsample:         %7.2f мс (%dx%d)\n",
           get_time_diff_ms(step_start, step_end),
           DEBUG_WIDTH, DEBUG_HEIGHT);
    /* 3. Сбор палитры */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    Palette palette_1 = palette_create(CHANNEL_VARIATIONS);
    build_palette(&downsample_image, &palette_1);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[PROC] build_palette:      %7.2f мс\n", get_time_diff_ms(step_start, step_end));
    /* 4. K-Means */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    Image quantized_1 = image_create(DEBUG_WIDTH, DEBUG_HEIGHT, 3);
    Palette best_colors = k_means_quantize(&downsample_image, &quantized_1, &palette_1, TARGET_COLORS, 20);
    pallette_free(&palette_1);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[PROC] k_means_quantize:   %7.2f мс\n", get_time_diff_ms(step_start, step_end));

    /* 5. Денойз */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    Image denoise_image = image_create(DEBUG_WIDTH, DEBUG_HEIGHT, 3);
    denoise(&quantized_1, &denoise_image, 1);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[PROC] denoise:            %7.2f мс\n", get_time_diff_ms(step_start, step_end));

    /* 7. Сохранение файла */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    image_write("result.jpg", &denoise_image);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[IO]   image_write:        %7.2f мс\n", get_time_diff_ms(step_start, step_end));

    clock_gettime(CLOCK_MONOTONIC, &total_end);
    printf("----------------------------------------\n");
    printf("[TOTAL] Общее время:       %7.2f мс\n", get_time_diff_ms(total_start, total_end));

    /* 8. Бинарные блобы и запись P1-маски */
    clock_gettime(CLOCK_MONOTONIC, &step_start);
    size_t pixel_count = (size_t) denoise_image.width * denoise_image.height;
    uint8_t *data = malloc(pixel_count * sizeof(uint8_t));
    to_binary_blobs(&denoise_image, data, best_colors.data[0]);

    FILE *mask_file = fopen("mask.pbm", "w");
    fprintf(mask_file, "P1\n%d %d\n", denoise_image.width, denoise_image.height);
    for (int y = 0; y < denoise_image.height; y++) {
        for (int x = 0; x < denoise_image.width; x++) {
            fputs(data[y * denoise_image.width + x] ? "1 " : "0 ", mask_file);
        }
        fputc('\n', mask_file);
    }
    fclose(mask_file);
    free(data);
    clock_gettime(CLOCK_MONOTONIC, &step_end);
    printf("[IO]   mask.pbm:           %7.2f мс\n", get_time_diff_ms(step_start, step_end));

    GlyphImage gi = glyph_image_create(400, 200);
    to_ascii(&denoise_image, &gi, best_colors.data, TARGET_COLORS, 7, " ");
    FILE *f = fopen("output.txt", "w");

    for (int y = 0; y < gi.height; y++) {
        for (int x = 0; x < gi.width; x++) {
            const char *g = gi.glyphs[y * gi.width + x];
            fputs(g ? g : " ", f);
        }
        fputc('\n', f);
    }

    fclose(f);
    return 0;
}
