#include "image.h"
#include "filter.h"
#include "utils.h"
#include "ascii.h"
#include "../external/stb_image_write.h"

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#define CHANNEL_VARIATIONS 4
#define PWD PREFERRED_WIDTH_DIVISOR
#define PHD PREFERRED_HEIGHT_DIVISOR
#define DENOISE_CLAMP 99

#define DEFAULT_DOWNSAMPLE (-1)
#define MAX_ASCII_RES 1024
#define MAX_ASCII_THRESHOLD 128
#define WIDE_BIAS_CLAMP 1000
#define INK_BIAS_CLAMP 20

#define STEPS "13"
#define MAX_READABLE_UTF 210041

#define FIND_ITERATIONS 1000

#define BENCHMARK(_label, _call) \
    do { \
        if (visualize_steps) {\
            struct timespec _step_start, _step_end; \
            clock_gettime(CLOCK_MONOTONIC, &_step_start); \
            _call; \
            clock_gettime(CLOCK_MONOTONIC, &_step_end); \
            if (!silent) fprintf(stderr, "%-80s %7.2lf ms\n", _label, get_time_diff_ms(_step_start, _step_end)); \
        }\
        else {\
            _call;\
        }\
    } while (0)

typedef enum {
    UNKNOWN,
    HELP,
    BUILTIN_GLYPHSET,
    DOWNSAMPLE_RES,
    ASCII_RES,
    K_MEANS_TRG_COLORS,
    DENOISE_RADIUS,
    ASCII_THRESHOLD,
    INPUT_PATH,
    OUTPUT_PATH,
    VISUALIZE_STEPS,
    NAME,
    INK_BIAS,
    NON_STRICT,
    SILENT,
    GLYPHSET_PATH,
    USE_WIDE,
    WIDE_BIAS,
} CmdArg;

static const char* DEFAULT_GLYPHS_CONFIG[] = {
    "⎹", "⎸", "▎", "⎮", "⎜", "⎟", "⌡", "╷", "⌠", "⇂", "⥏", "⟯", "↿", "╽", "╿", "↾",
    "⟮", "⎻", "⎼", "¯", "▔", "⎽", "─", "—", "‗", "▁", "⎺", "_", "╾", "╼", "▱", "━",
    "↼", "‒", "⇁", "═", "↽", "⇀", "╲", "╱", "⧸", "\\", "⋱", "⧹", "⧵", "∕", "/", "∖",
    "⋰", "⌜", "⌝", "⌞", "⌟", "¬", "└", "⨼", "⨽", "╯", "⊿", "⸀", "╮", "⊾", "L", "˪",
    "⭏", "┘", "˥", "⦜", "┕", "∟", "≁", "⌊", "┚", "┐", "┌", "╘", "]", "⧷", "√", "↗",
    "⥡", "⇖", "↖", "⌈", "⁊", "r", "˨", "⋪", "╜", "⌉", "⇗", "┙", "⤹", "˩", "┴", "⦍",
    "˫", "╨", "∍", "╕", "┒", "˦", "⇽", "Z", "┸", "⊦", "┎", "⤸", "↘", "≭", "┦", "┤",
    "⦐", "◰", "P", "┬", "⊨", "⫞", "╙", "E", "b", "≰", "⥘", "╞", "[", "╰", "^", "⦦",
    "⦧", "╭", "⦣", "⦟", "⨡", "∠", "⋌", "⁓", "‿", "⋋", "∿", "⎨", "~", "∼", "◸", "◺",
    "∫", "⎝", "◹", "⧼", "⎛", "≽", "∛", "⎤", "⟭", ")", "◿", "⩋", "⧽", "⎬", "⟨", "╩",
    "≿", "⟪", "⟦", "⌣", "≾", "◡", "⬯", "⨞", "⨔", "⪒", "⎡", "∑", "‱", "⪋", "⌢", "⋡",
    "⎦", "⋩", "⋞", "⪑", "⪓", "⟧", "⨫", "⨬", "⟓", "⦝", "⟔", "╌", "╍", "⧶", "⁐", "≒",
    "≟", "⩫", "╚", "j", "↸", "≓", "⨛", "⋖", "╧", "⨜", "⊆", "≚", "⨩", "≂", "┆", "⪐",
    "╎", "⨒", "⧡", "╤", "◞", "◟", "◝", "◜", "╺", "ｰ", "ﾆ", "｣", "ﾍ", "ﾉ", "｢", "ｺ",
    "ｲ", "ﾌ", "ﾝ", "ｳ", "ﾋ", "ﾐ", "ﾄ", "ﾂ", "ﾏ", "ﾛ", "ｱ", "ｼ", "ｽ", "ﾖ", "ﾇ", "ﾘ",
    "ﾊ", "ﾑ", "ｿ", "一", "ー", "＿", "￣", "〜", "／", "＼", "乀", "乁", "ノ", "乚", "乛", "厂",
    "く", "へ", "（", "〱", "〲", "ﾲ", "˭", "▰", "꜡", "꜠", "ར", "☁", "༽", "␣", "⹔", "Ґ",
    "ᔼ", "⸌", "⹝", "⹓", "ᐟ", "֊", "ᓲ", "˜", "՟", "ᚡ", "ꟳ", "꙳", "Ꝇ", "⁁", "ᐹ", "⎕",
    "ᐥ", "ϛ", "ᐨ", "ᓐ", "ⲻ", "ʺ", "ﾤ", "ⵃ", "ᣚ", "ཪ", "Ḹ", "､", "ᔥ", "꓾", "⳿", "▘",
    "ʗ", "ꓺ", "ໂ", "ⳑ", "ᛕ", "‶", "ე", "˻", "ᛁ", "ˏ", "↹", "ᢿ", "῝", "Ⲻ", "ⴈ", "⌁",
    "◗", "ᦚ", "↳", "ᖟ", "ꟴ", "ᾖ", "᎖", "ᒷ", "༥", "▝", "⌐", "⹀", "ᐠ", "ᱻ", "ᣖ", "ﾜ",
    "⊏", "៰", "ꛤ", "ⴢ", "ᔷ", "⸂", "ᶫ", "Ὑ", "ⴆ", "Ͱ", "լ", "᧒", "ȡ", "ꞏ", "ᓻ", "፡",
    "⌒", "Ꚗ", "ᔧ", "⍤", "⍨", "᧓", "Ϧ", "⸋", "Ż", "⳨", "ⴀ", "ᒤ", "◦", "ꓒ", "Ⳇ", "Ṯ",
    "ไ", "⌍", "ས", "ᚺ", "˘", "ᓺ", "⁾", "ᚵ", "ↅ", "ད", "Ύ", "ʹ", "ᴧ", "ᓴ", "〈", "ᔃ",
    "Ⱳ", "Ⲣ", "ҍ", "Ř", "Ὦ", "ꝿ", "⋄", "ᔨ", "ྉ", "༕", "ɼ", "ᣴ", "ᴾ", "ᢵ", "ˁ", "ꓻ",
    "‹", "Ⲁ", "ᣞ", "ǉ", "༸", "ₓ", "♭", "ᛈ", "ᓒ", "ₘ", "ﾾ", "ꙁ", "ᐣ", "›", "ᚪ", "ჯ",
    "Ⲍ", "ӷ", "ᴶ", "ﾻ", "◻", "უ", "ᐳ", "┹", "ᐴ", "Ⳓ", "˽", "ᦸ", "▭", "┺", "࿀", "Ω",
    "Γ", "ⴑ", "ꙿ", "Ⴀ", "໔", "╬", "⦥", "ⳇ", "ᚴ", "โ", "ᖮ", "ᶘ", "Ϛ", "ᒵ", "༢",
    "⧑", "ｨ", "″"
};

#define DEFAULT_GLYPHS_LENGTH (sizeof(DEFAULT_GLYPHS_CONFIG) / sizeof(char *))

static char** str_ptr_buf = NULL;
static int buf_cnt = 0;

static const char* con_str(const char* str1, const char* str2) {
    if (str_ptr_buf == NULL) {
        str_ptr_buf = malloc(1000 * sizeof(char*));
    }

    unsigned long len1 = strlen(str1);
    unsigned long len2 = strlen(str2);
    char* buffer = malloc(len1 + len2 + 1);

    buffer[0] = '\0';
    strcat(buffer, str1);
    strcat(buffer, str2);
    buffer[len1 + len2] = '\0';

    str_ptr_buf[buf_cnt++] = buffer;
    return buffer;
}

static double get_time_diff_ms(struct timespec t_start, struct timespec t_end) {
    return (double)(t_end.tv_sec - t_start.tv_sec) * 1000 +
        (double)(t_end.tv_nsec - t_start.tv_nsec) / 1e6;
}

static int parse_long(const char* str, long* out) {
    char* end;
    errno = 0;
    long val = strtol(str, &end, 10);

    if (end == str || *end != '\0' || errno == ERANGE) {
        return 1;
    }

    *out = val;
    return 0;
}

static int test_path_in(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "[Error] Cannot open %s: %s.\n", path, strerror(errno));
        return 0;
    }
    fclose(f);
    return 1;
}

static int tets_path_out(const char* path) {
    FILE* f = fopen(con_str(path, "/tmp pmt.tmp"), "w");
    if (!f) {
        fprintf(stderr, "[Error] Cannot write %s: %s.\n", path, strerror(errno));
        return 0;
    }
    fclose(f);
    remove(con_str(path, "/tmp pmt.tmp"));
    return 1;
}


static CmdArg parse_argument(const char* arg) {
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) return HELP;
    if (!strcmp(arg, "--builtin-glyphset") || !strcmp(arg, "-bg")) return BUILTIN_GLYPHSET;
    if (!strcmp(arg, "--input") || !strcmp(arg, "-i")) return INPUT_PATH;
    if (!strcmp(arg, "--output-dir") || !strcmp(arg, "-o")) return OUTPUT_PATH;
    if (!strcmp(arg, "--name") || !strcmp(arg, "-n")) return NAME;
    if (!strcmp(arg, "--glyphset") || !strcmp(arg, "-g")) return GLYPHSET_PATH;

    if (!strcmp(arg, "--downsample-res") || !strcmp(arg, "-d")) return DOWNSAMPLE_RES;
    if (!strcmp(arg, "--ascii-res") || !strcmp(arg, "-a")) return ASCII_RES;
    if (!strcmp(arg, "--k-means-colors") || !strcmp(arg, "-c")) return K_MEANS_TRG_COLORS;
    if (!strcmp(arg, "--denoise-radius") || !strcmp(arg, "-r")) return DENOISE_RADIUS;
    if (!strcmp(arg, "--ascii-threshold") || !strcmp(arg, "-t")) return ASCII_THRESHOLD;
    if (!strcmp(arg, "--ink-bias") || !strcmp(arg, "-b")) return INK_BIAS;
    if (!strcmp(arg, "--exclude-wide") || !strcmp(arg, "-ew")) return USE_WIDE;
    if (!strcmp(arg, "--wide-bias")) return WIDE_BIAS;

    if (!strcmp(arg, "--non-strict-downsample") || !strcmp(arg, "-nd")) return NON_STRICT;
    if (!strcmp(arg, "--visualize-steps") || !strcmp(arg, "-v")) return VISUALIZE_STEPS;
    if (!strcmp(arg, "--silent") || !strcmp(arg, "-S")) return SILENT;

    return UNKNOWN;
}

static void print_help(void) {
    printf("Usage: glyphomania --input <path> [--ascii-res <w> <h>] [options]\n\n");
    printf("Converts an image into an ASCII glyph mosaic using downsampling, k-means\n");
    printf("color quantization, morphological closing, blob extraction, and distance-field\n");
    printf("glyph matching.\n\n");

    printf("Defaults:\n");
    printf("  --ascii-res 300 100          --downsample-res <input>   --k-means-colors 4\n");
    printf("  --denoise-radius 3           --ascii-threshold 8        --ink-bias 0\n");
    printf("  --wide-bias 300              --name ASCII.txt           --glyphset (call --bg to check)\n");
    printf("  --output-dir none (stdout)   --exclude-wide off         --non-strict-downsample off\n");
    printf("  --visualize-steps off        --silent off\n\n");

    printf("I/O options:\n");
    printf("  -i, --input <path>            Input image path (required, any stb_image format:\n");
    printf("                                JPEG, PNG, BMP, GIF, TGA, PSD, HDR, PIC, PNM)\n");
    printf("  -o, --output-dir <dir>        Directory for the ASCII file and debug dumps (default: ./).\n");
    printf("                                Without this option the ASCII art is printed to stdout\n");
    printf("                                and --name is ignored\n");
    printf("  -n, --name <file>             Output file name inside --output-dir (default: ASCII.txt)\n");
    printf("  -g, --glyphset <path>         UTF-8 text file with the glyphs to match against\n");
    printf("                                (default: built-in set of %zu glyphs).\n", DEFAULT_GLYPHS_LENGTH);
    printf("                                Spaces, tabs and newlines are ignored, duplicates are removed.\n\n");

    printf("Pipeline tuning:\n");
    printf("  -a, --ascii-res <w> <h>       ASCII grid size in characters (default: 300x100, max: %dx%d).\n",
           MAX_ASCII_RES, MAX_ASCII_RES);
    printf("                                Each character cell is %dx%d px in full-res space\n", PWD, PHD);
    printf("  -d, --downsample-res <w> <h>  Pre-quantization working resolution (default: input image res).\n");
    printf("                                Clamped with a warning if larger than input or ASCII grid\n");
    printf("  -nd, --non-strict-downsample  Do not clamp --downsample-res: allow upscaling beyond the input\n");
    printf("                                and the ASCII grid. Slower, uses more memory, adds no detail\n");
    printf("                                but can be used to tune denoise more precisely\n");
    printf("  -c, --k-means-colors <n>      Number of palette clusters (2-64, default: 4, recommended: 2-8)\n");
    printf("  -r, --denoise-radius <n>...   Odd radii (1-%d) for cascade morphological closing (default: 3).\n",
           DENOISE_CLAMP);
    printf("                                Each radius is one pass, applied in the given order.\n");
    printf("                                Consumes values until next flag (e.g. -r 7 5 3).\n");
    printf("                                Large radii get slow quickly.\n");
    printf("                                Recommended: decreasing odd cascade from 9..3\n");
    printf("                                radius 1 will not apply any changes.\n");
    printf("  -t, --ascii-threshold <n>     Minimum edge pixels in a slice required to pick a glyph\n");
    printf("                                instead of empty space (1-%d, default: 4, recommended: 3-12)\n",
           MAX_ASCII_THRESHOLD - 1);
    printf("                                near to %d value means no glyph will be chosen\n", MAX_ASCII_THRESHOLD);
    printf("  -b, --ink-bias <n>            Biases distance transform to shift glyph density/coverage\n");
    printf("                                (-20..20, default: 0). Negative values favor dense,\n");
    printf("                                noisy glyphs that fill more cell area; positive values\n");
    printf("                                favor sparse, minimal glyphs\n");
    printf("  -ew, --exclude-wide           Do not use wide glyphs (they occupy two neighbouring cells,\n");
    printf("                                one half per cell). Off by default\n");
    printf("      --wide-bias <n>           Strength of the wide-glyph bias (0..1000, default: 300).\n");
    printf("                                Tune it if wide glyphs are picked too rarely or too often\n\n");

    printf("Diagnostics:\n");
    printf("  -v, --visualize-steps         Export intermediate files (Downsampled.png, Quantised.png,\n");
    printf("                                Denoised.png, Binary.pbm) to --output-dir (default: ./).\n");
    printf("                                Stage timings are also printed to stderr unless --silent is set\n");
    printf("  -S, --silent                  Suppress warnings and stage timings (errors are still printed,\n");
    printf("                                the ASCII output is not affected)\n");
    printf("  -h, --help                    Show this help message and exit\n\n");
    printf("  -bg, --builtin-glyphset       Show built-in set of %zu glyphs\n\n", DEFAULT_GLYPHS_LENGTH);


    printf("Examples:\n");
    printf("  Minimal run (ASCII art goes to stdout):\n");
    printf("  glyphomania -i input.png -a 200 100\n\n");
    printf("  Custom number of colors and denoise radius:\n");
    printf("  glyphomania -i input.png -a 200 100 --k-means-colors 3 --denoise-radius 3\n\n");
    printf("  Full example (every option except --exclude-wide):\n");
    printf("  glyphomania --input input.png \\\n");
    printf("              --ascii-res 200 100 \\\n");
    printf("              --downsample-res 800 1000 --non-strict-downsample \\\n");
    printf("              --k-means-colors 3 \\\n");
    printf("              --denoise-radius 5 3 \\\n");
    printf("              --ascii-threshold 6 --ink-bias 3 --wide-bias 300 \\\n");
    printf("              --glyphset my_glyphs.txt \\\n");
    printf("              --visualize-steps \\\n");
    printf("              --output-dir ./out --name result.txt\n");
}

static void print_builtin_glyphset() {
    for (int i = 0; i < DEFAULT_GLYPHS_LENGTH; i++) {
        if (i % 80 == 0) printf("\n");
        printf("%s ", DEFAULT_GLYPHS_CONFIG[i]);
    }
}

static wint_t* parse_default_glyphs_config() {
    setlocale(LC_ALL, "");

    wint_t* local_palette = malloc(DEFAULT_GLYPHS_LENGTH * sizeof(wint_t));

    for (int i = 0; i < DEFAULT_GLYPHS_LENGTH; i++) {
        wchar_t wc;
        mbtowc(&wc, DEFAULT_GLYPHS_CONFIG[i], MB_CUR_MAX);
        local_palette[i] = (wint_t)wc;
    }

    return local_palette;
}

//TODO убрать повторяющиеся глифы
//TODO Файл может быть пустым, i - 1 -> 0 - 1
static int parse_glyphs_config(const char* path, wint_t** glyphs_palette) {
    int buf = 128;
    wint_t* local_palette = calloc(buf, sizeof(wint_t));
    uint8_t* hashmap = calloc(MAX_READABLE_UTF, sizeof(uint8_t));

    FILE* f = fopen(path, "rb");
    wint_t codepoint;
    int i = 0;

    setlocale(LC_ALL, "");

    while ((codepoint = fgetwc(f)) != WEOF) {
        if (codepoint > MAX_READABLE_UTF) {
            char local_glyph[5] = {0};
            wcrtomb(local_glyph, codepoint, &(mbstate_t){0});
            fprintf(stderr, "[Error] Unsupported glyph: %s in %s", local_glyph, path);
            exit(1);
        }
        if (codepoint == 32 || codepoint == 9 || codepoint == 10 || codepoint == 13 || codepoint == 65279 ||
            hashmap[codepoint] > 0) continue;

        local_palette[i++] = codepoint;
        hashmap[codepoint]++;

        if (i >= buf) {
            local_palette = realloc(local_palette, sizeof(wint_t) * buf * 2);
            buf *= 2;
        }
    }

    if (i == 0) {
        fprintf(stderr, "[Error] Empty --glyphset file: %s", path);
        exit(1);
    }

    *glyphs_palette = local_palette;
    free(hashmap);
    fclose(f);
    return i;
}

static int get_char_index(const Header* header, wint_t glyph) {
    int left = 0;
    int right = header->length - 1;
    int index = right / 2;
    wint_t curr = header->records[index].glyph_codepoint;
    int i = 0;

    while (curr != glyph && i < FIND_ITERATIONS) {
        if (glyph < curr) right = index - 1;
        if (glyph > curr) left = index + 1;
        index = (left + right) / 2;
        if (index < 0 || index >= header->length) {
            i++;
            continue;
        }
        curr = header->records[index].glyph_codepoint;
        i++;
    }

    if (i >= FIND_ITERATIONS) fprintf(stderr, "[Error] Unsupported glyph: %d", glyph), exit(1);

    if (index > 0 && header->records[index - 1].glyph_codepoint == glyph) return index - 1;

    return index;
}

static int glyphs_palette_create(const Header* header, GlyphTile* tiles, char (**glyphs)[5],
                                 const wint_t* glyphs_palette, int palette_length) {
    char (*local_glyphs)[5] = calloc(palette_length * 2, sizeof(char[5]));

    int narrow_offset = 0;
    for (int i = 0; i < palette_length; i++) {
        wint_t glyph = glyphs_palette[i];
        const Record* rec = &header->records[get_char_index(header, glyph)];
        if (!rec->is_wide) {
            tiles[narrow_offset].bitmap = rec->bitmap;
            wcrtomb(local_glyphs[narrow_offset], rec->glyph_codepoint, &(mbstate_t){0});
            tiles[narrow_offset].glyph = local_glyphs[narrow_offset];
            narrow_offset++;
        }
    }

    int wide_offset = narrow_offset;
    for (int i = 0; i < palette_length; i++) {
        wint_t glyph = glyphs_palette[i];
        int index = get_char_index(header, glyph);
        const Record* rec = &header->records[index];
        if (rec->is_wide) {
            for (int side = 0; side < 2; side++) {
                tiles[wide_offset].bitmap = rec->bitmap;
                wcrtomb(local_glyphs[wide_offset], rec->glyph_codepoint, &(mbstate_t){0});
                tiles[wide_offset].glyph = local_glyphs[wide_offset];
                if (side == 0) rec = &header->records[index + 1];
                wide_offset++;
            }
        }
    }

    *glyphs = local_glyphs;
    return narrow_offset;
}

static void resolve_resolution(const Image* src, long ascii_w, long ascii_h, long* ds_w, long* ds_h, int non_strict,
                               int silent) {
    long src_w = src->width;
    long src_h = src->height;
    long full_w = ascii_w * PWD;
    long full_h = ascii_h * PHD;

    if (*ds_w == DEFAULT_DOWNSAMPLE) {
        *ds_w = src_w;
        if (!silent) fprintf(stderr, "[Warning] Downsample: Using --input image res as downsampled\n");
    }
    if (*ds_h == DEFAULT_DOWNSAMPLE) {
        *ds_h = src_h;
    }

    if ((*ds_w > src_w || *ds_h > src_h) && non_strict) {
        if (!silent)
            fprintf(
                stderr,
                "[Warning] Downsample: Using non-optimal --downsample-res. Upscaling instead of downsampling\n");
        return;
    }

    if (*ds_w > src_w && src_w <= full_w) {
        if (!silent)
            fprintf(stderr, "[Warning] Downsample: --downsample-res width (%ld) exceeds source (%ld), using %ld\n",
                    *ds_w, src_w, src_w);
        *ds_w = src_w;
    }
    else if (*ds_w == src_w && *ds_w > full_w) {
        if (!silent)
            fprintf(stderr, "[Warning] Downsample: --input image res width (%ld) exceeds ASCII grid (%ld), using %ld\n",
                    *ds_w, full_w, full_w);
        *ds_w = full_w;
    }
    else if (*ds_w != src_w && *ds_w > full_w) {
        if (!silent)
            fprintf(stderr, "[Warning] Downsample: --downsample-res width (%ld) exceeds ASCII grid (%ld), using %ld\n",
                    *ds_w, full_w, full_w);
        *ds_w = full_w;
    }

    if (*ds_h > src_h && src_h <= full_h) {
        if (!silent)
            fprintf(stderr, "[Warning] Downsample: --downsample-res height (%ld) exceeds source (%ld), using %ld\n",
                    *ds_h, src_h, src_h);
        *ds_h = src_h;
    }
    else if (*ds_h == src_h && *ds_h > full_h) {
        if (!silent)
            fprintf(
                stderr, "[Warning] Downsample: --input image res height (%ld) exceeds ASCII grid (%ld), using %ld\n",
                *ds_h, full_h, full_h);
        *ds_h = full_h;
    }
    else if (*ds_h != src_h && *ds_h > full_h) {
        if (!silent)
            fprintf(stderr, "[Warning] Downsample: --downsample-res height (%ld) exceeds ASCII grid (%ld), using %ld\n",
                    *ds_h, full_h, full_h);
        *ds_h = full_h;
    }

    if (full_w > *ds_w || full_h > *ds_h) {
        if (!silent)
            fprintf(stderr, "[Warning] ASCII grid: %ldx%ld px, downsampled: %ldx%ld. "
                    "Downsampled will be interpolated\n",
                    full_w, full_h, *ds_w, *ds_h);
    }
}

static void add_blobs(const uint8_t* data_new, uint8_t* data_dump, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        data_dump[i] = data_dump[i] | data_new[i];
    }
}

static void con_ctx(AsciiCtx* ctx1, AsciiCtx ctx2) {
    ctx1->narrow_cnt += ctx2.narrow_cnt;
    ctx1->wide_cnt += ctx2.wide_cnt;
}

static void visualise_step(Image* image, const char* path, int boolean_visualise) {
    if (boolean_visualise) {
        int err = stbi_write_png(path, image->width, image->height, image->channels, image->data, image->width * 3);
        if (!err) {
            perror(path);
        }
    }
}

static void visualise_blobs(const uint8_t* data, int width, int height, const char* path,
                            int visualise_steps) {
    if (!visualise_steps) return;

    FILE* f = fopen(path, "w");
    fprintf(f, "P1\n%d %d\n", width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data[y * width + x] ? fputs("1 ", f) : fputs("0 ", f);
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void r_ascii(FILE* f, const char** data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fputs(data[y * width + x], f);
        }
        fputc('\n', f);
    }
}

int main(int argc, char* argv[]) {
    const char* input_path = NULL;
    const char* glyphset_path = "Default";
    const char* output_path = "./";
    const char* name = "ASCII.txt";
    const char* denoise_args = "";

    long ascii_w = 300;
    long ascii_h = 100;
    long downsample_w = DEFAULT_DOWNSAMPLE;
    long downsample_h = DEFAULT_DOWNSAMPLE;
    long k_means_colors = 4;
    long k_means_iters = 10;
    long k_means_stride = 8;
    long ascii_threshold = 4;
    long ink_bias = 0;
    long wide_bias = 300;

    long* denoise_radius_vals = malloc(4 * sizeof(long));
    denoise_radius_vals[0] = 3;
    int vals_cnt = 1;

    int visualize_steps = 0;
    int output = 0;
    int glyphset = 0;
    int non_strict = 0;
    int silent = 0;
    int use_wide = 1;

    AsciiCtx ctx = {.wide_cnt = 0, .narrow_cnt = 0};
    struct timespec total_start, total_end;

#if defined(__CLION_IDE__)
    static const unsigned char rodata[] = {0}; //fvck jetbrains FIX YOUR IDE DAMNIT
#else
    static const uint8_t rodata[] = {
#embed "../resources/glyph_bitmap.bin"
    };
#endif

    const Header* header = (const Header*)rodata;

    for (int i = 1; i < header->length; ++i) {
        uint32_t prev_code = header->records[i - 1].glyph_codepoint;
        uint32_t curr_code = header->records[i].glyph_codepoint;
        uint8_t curr_wide = header->records[i].is_wide;
        if (curr_wide) {
            assert(curr_code >= prev_code);
        }
        else {
            assert(curr_code > prev_code);
        }
    }

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

        case BUILTIN_GLYPHSET:
            print_builtin_glyphset();
            return 0;

        case INPUT_PATH:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --input requires a path.\n");
                return 1;
            }
            input_path = argv[i];
            if (!test_path_in(input_path)) {
                return 1;
            }
            break;

        case OUTPUT_PATH:
            if (++i >= argc || argv[i][0] == '-') {
                fprintf(stderr, "[Error] --output-dir requires a path.\n");
                return 1;
            }
            output_path = con_str(argv[i], "/");
            if (!tets_path_out(output_path)) {
                return 1;
            }
            output = 1;
            break;

        case NAME:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --name requires a non-empty string.\n");
                return 1;
            }
            name = argv[i];
            if (!strcmp(argv[i], "")) {
                fprintf(stderr, "[Error] --name can not be empty.\n");
                return 1;
            }
            break;

        case GLYPHSET_PATH:
            if (++i >= argc || argv[i][0] == '-') {
                fprintf(stderr, "[Error] --glyphset requires a path.\n");
                return 1;
            }
            glyphset_path = argv[i];
            if (!test_path_in(glyphset_path)) {
                return 1;
            }
            glyphset = 1;
            break;

        case DOWNSAMPLE_RES:
            if (i + 2 >= argc) {
                fprintf(stderr, "[Error] --downsample-res requires width height.\n");
                return 1;
            }
            if (parse_long(argv[++i], &downsample_w) || parse_long(argv[++i], &downsample_h) || downsample_w <= 0
                || downsample_h <= 0) {
                fprintf(stderr, "[Error] Invalid dimensions for --downsample-res.\n");
                return 1;
            }
            break;

        case ASCII_RES:
            if (i + 2 >= argc) {
                fprintf(stderr, "[Error] --ascii-res requires width height.\n");
                return 1;
            }
            if (parse_long(argv[++i], &ascii_w) || parse_long(argv[++i], &ascii_h)
                || ascii_w <= 0 || ascii_h <= 0) {
                fprintf(stderr, "[Error] Invalid dimensions for --ascii-res.\n");
                return 1;
            }
            if (ascii_w > MAX_ASCII_RES || ascii_h > MAX_ASCII_RES) {
                fprintf(stderr, "[Error] Crazy? Ascii res value exceeds maximum allowed. \n"
                        "Enter width and height < %d\n", MAX_ASCII_RES);
                return 1;
            }
            break;

        case K_MEANS_TRG_COLORS:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --k-means-colors requires a number.\n");
                return 1;
            }
            if (parse_long(argv[i], &k_means_colors) || k_means_colors > 256 / CHANNEL_VARIATIONS
                || k_means_colors < 2) {
                fprintf(stderr, "[Error] Invalid value for --k-means-colors: %s."
                        " Enter an integer between 2 and 64."
                        " The best value is between 2 and 8.\n", argv[i]);
                return 1;
            }
            break;

        case DENOISE_RADIUS:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --denoise-radius requires an odd positive number <= %d.\n", DENOISE_CLAMP);
                return 1;
            }
            int cnt = 0;
            for (int j = i; j < argc && argv[j][0] != '-'; j++) {
                cnt++;
            }
            if (!cnt) {
                fprintf(stderr, "[Error] --denoise-radius requires odd positive number <= %d.\n", DENOISE_CLAMP);
                return 1;
            }

            free(denoise_radius_vals);
            denoise_radius_vals = malloc(cnt * sizeof(long));
            for (int j = 0; j < cnt; j++) {
                long denoise_radius;
                if (parse_long(argv[i + j], &denoise_radius) || denoise_radius < 1 || denoise_radius % 2 == 0) {
                    fprintf(stderr, "[Error] Invalid value for --denoise-radius: %s."
                            " Enter an odd positive number <= %d."
                            " The best value is between 3 and 9.\n", argv[i + j], DENOISE_CLAMP);
                    return 1;
                }
                if (denoise_radius > DENOISE_CLAMP) {
                    fprintf(stderr, "[Error] Invalid value for --denoise-radius: %s."
                            " You are going to be waiting for ages. Enter an odd positive number <= %d."
                            " The best value is between 3 and 9.\n", argv[i + j], DENOISE_CLAMP);
                    return 1;
                }
                denoise_radius_vals[j] = denoise_radius;
                denoise_args = con_str(denoise_args, con_str(argv[i + j], " "));
            }
            vals_cnt = cnt;
            i += cnt - 1;
            break;

        case ASCII_THRESHOLD:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --ascii-threshold requires a number <= %d.\n", MAX_ASCII_THRESHOLD);
                return 1;
            }
            if (parse_long(argv[i], &ascii_threshold) || ascii_threshold <= 0 || ascii_threshold >
                MAX_ASCII_THRESHOLD) {
                fprintf(stderr, "[Error] Invalid value for --ascii-threshold: %s."
                        " Enter a positive number <= %d. The best value is between 3 and 12.\n", argv[i],
                        MAX_ASCII_THRESHOLD);
                return 1;
            }
            break;

        case INK_BIAS:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --ink-bias requires a number %d <= value <= %d.\n", -INK_BIAS_CLAMP,
                        INK_BIAS_CLAMP);
                return 1;
            }
            if (parse_long(argv[i], &ink_bias) || ink_bias < -INK_BIAS_CLAMP || ink_bias > INK_BIAS_CLAMP) {
                fprintf(stderr, "[Error] Invalid value for --ink-bias: %s."
                        " Value must be between %d and %d.\n", argv[i], -INK_BIAS_CLAMP, INK_BIAS_CLAMP);
                return 1;
            }
            break;

        case USE_WIDE:
            use_wide = 0;
            break;

        case WIDE_BIAS:
            if (++i >= argc) {
                fprintf(stderr, "[Error] --wide-bias requires a number 0 <= value <= %d.\n", WIDE_BIAS_CLAMP);
                return 1;
            }
            if (parse_long(argv[i], &wide_bias) || wide_bias < 0 || wide_bias > WIDE_BIAS_CLAMP) {
                fprintf(stderr, "[Error] Invalid value for --wide-bias: %s."
                        " Value must be between 0 and %d.\n", argv[i], WIDE_BIAS_CLAMP);
                return 1;
            }
            break;

        case NON_STRICT:
            non_strict = 1;
            break;

        case VISUALIZE_STEPS:
            visualize_steps = 1;
            break;

        case SILENT:
            silent = 1;
            break;

        default:
            fprintf(stderr, "[Error] Unknown argument: %s.\n", argv[i]);
            return 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &total_start);

    Image src_image;
    if (!image_load(&src_image, input_path)) {
        if (input_path == NULL) {
            fprintf(stderr, "[Error] --input is required\n");
            return 1;
        }

        fprintf(stderr, "[Error] Cannot load image %s\n", input_path);
        return 1;
    }

    if (!output) name = "stdout.";

    resolve_resolution(&src_image, ascii_w, ascii_h, &downsample_w, &downsample_h, non_strict, silent);

    if (visualize_steps && !silent) {
        fprintf(stderr, "%-10s %s (%dx%d)\n", "Input:", input_path, src_image.width, src_image.height);
        fprintf(stderr, "%-10s %s\n", "Output:", output ? con_str(output_path, name) : "stdout");
        fprintf(stderr, "%-10s %ldx%ld chars, cell %dx%d px, full-res %ldx%ld px\n", "Grid:",
                ascii_w, ascii_h, PWD, PHD, ascii_w * PWD, ascii_h * PHD);
        fprintf(stderr, "%-10s threshold %ld, ink bias %ld, wide glyphs %s, wide bias %ld\n", "Match:",
                ascii_threshold, ink_bias, use_wide ? "on" : "off", wide_bias);
        fprintf(stderr,
                "-------------------------------------------------------------------------------------------\n");
    }

    char buf[128];
    GlyphTile* tiles = malloc(sizeof(GlyphTile) * header->length);
    wint_t* glyph_palette = glyphset ? NULL : parse_default_glyphs_config();

    char (*glyphs_storage)[5];

    snprintf(buf, sizeof(buf), "%s", glyphset ? glyphset_path : "built-in set");
    BENCHMARK(con_str("[01/", con_str(con_str(STEPS, "] Select glyphs: "), buf)),
              int length;
              length = glyphset ? parse_glyphs_config(glyphset_path, &glyph_palette) : DEFAULT_GLYPHS_LENGTH;
              int wide_offset = glyphs_palette_create(header, tiles, &glyphs_storage, glyph_palette, length);
              parse_rodata(tiles, wide_offset, wide_offset + (length - wide_offset) * 2););

    free(glyph_palette);
    glyph_palette = NULL;

    Image downsample_image = image_create((int)downsample_w, (int)downsample_h, DESIRED_CHANNELS);
    snprintf(buf, sizeof(buf), "%dx%d -> %ldx%ld", src_image.width, src_image.height, downsample_w, downsample_h);
    BENCHMARK(con_str("[02/", con_str(con_str(STEPS, "] Downsample: Area Average, "), buf)),
              resample_intersection(&src_image, &downsample_image, 1));
    BENCHMARK(con_str("[03/", con_str(con_str(STEPS, "] Visualize: "), con_str(output_path, "Downsampled.png"))),
              visualise_step(&downsample_image, con_str(output_path, "Downsampled.png"), visualize_steps));

    image_free(&src_image);

    Palette palette = palette_create(CHANNEL_VARIATIONS);
    BENCHMARK(con_str("[04/", con_str(STEPS, "] Palette: Histogram Sampling")),
              build_palette(&downsample_image, &palette));

    Image quantized = image_create((int)downsample_w, (int)downsample_h, DESIRED_CHANNELS);
    Palette best_colors;
    snprintf(buf, sizeof(buf), "%ld colors, %ld iters, every %ldth pixel used",
             k_means_colors, k_means_iters, k_means_stride);
    BENCHMARK(con_str("[05/", con_str(con_str(STEPS, "] K-Means: Color Quantization, "), buf)),
              best_colors = k_means_quantize(
                  &downsample_image, &quantized, &palette,
                  (int) k_means_colors, (int) k_means_iters));
    BENCHMARK(con_str("[06/", con_str(con_str(STEPS, "] Visualize: "), con_str(output_path, "Quantised.png"))),
              visualise_step(&quantized, con_str(output_path, "Quantised.png"), visualize_steps));
    pallette_free(&palette);

    image_free(&downsample_image);

    Image denoised = image_create((int)downsample_w, (int)downsample_h, DESIRED_CHANNELS);
    Image* curr_denoised = NULL;
    if (*denoise_args) snprintf(buf, sizeof(buf), "%s", denoise_args);
    else snprintf(buf, sizeof(buf), "%ld", denoise_radius_vals[0]);
    BENCHMARK(con_str("[07/", con_str(con_str(STEPS, "] Denoise: Majority filter, radii in order: "), buf)),
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
    BENCHMARK(con_str("[08/", con_str(con_str(STEPS, "] Visualize: "), con_str(output_path, "Denoised.png"))),
              visualise_step(curr_denoised, con_str(output_path, "Denoised.png"), visualize_steps));

    GlyphImage glyph_image = glyph_image_create((int)ascii_w, (int)ascii_h);
    int full_w = glyph_image.width * PWD;
    int full_h = glyph_image.height * PHD;

    uint8_t* data = calloc(curr_denoised->width * curr_denoised->height, sizeof(uint8_t));
    uint8_t* data_resample = calloc(full_w * full_h, sizeof(uint8_t));
    uint8_t* data_edges = calloc(full_w * full_h, sizeof(uint8_t));
    uint8_t* data_dump = calloc(full_w * full_h, sizeof(uint8_t));

    FILE* f;
    if (output) {
        f = fopen(con_str(output_path, name), "w");
    }
    else {
        f = stdout;
    }

    for (int color_i = 0; color_i < k_means_colors; color_i++) {
        Pixel color = best_colors.data[color_i];

        snprintf(buf, sizeof(buf), "%d/%ld, rgb(%d, %d, %d)", color_i + 1, k_means_colors, color.r, color.g, color.b);
        if (visualize_steps && !silent) fprintf(stderr, "\nColor %s\n", buf);

        BENCHMARK(con_str("    [09/", con_str(STEPS, "] Blobs: Upsample + Mask + Edges")),
                  to_binary_blobs(curr_denoised, data, color);
                  blobs_bilinear_resample(curr_denoised, data, data_resample, full_w, full_h);
                  blob_find_edges(data_resample, data_edges, full_w, full_h););
        BENCHMARK(con_str("    [10/", con_str(STEPS, "] Accumulate: Binary Frame ")),
                  add_blobs(data_edges, data_dump, full_w, full_h));
        BENCHMARK(con_str("    [11/", con_str(STEPS, "] ASCII: Compare chunks with glyphs")),
                  con_ctx(&ctx, to_ascii(&glyph_image, data_edges, (int) ascii_threshold, " ", ink_bias, use_wide,
                      wide_bias)));

        memset(data, 0, curr_denoised->width * curr_denoised->height * sizeof(uint8_t));
        memset(data_resample, 0, full_w * full_h * sizeof(uint8_t));
        memset(data_edges, 0, full_w * full_h * sizeof(uint8_t));
    }

    free(data);
    free(data_resample);
    free(data_edges);
    image_free(&denoised);
    image_free(&quantized);
    free(best_colors.data);

    BENCHMARK(con_str("[12/", con_str(con_str(STEPS, "] Output: Binary Dump, "), con_str(output_path, "Binary.pbm"))),
              visualise_blobs(data_dump, full_w, full_h, con_str(output_path,
                  con_str("Binary", ".pbm")), visualize_steps));

    snprintf(buf, sizeof(buf), ", res: %ldx%ld, wide glyphs: %d, narrow glyphs: %d",
             ascii_w, ascii_h, ctx.wide_cnt, ctx.narrow_cnt);
    BENCHMARK(
        con_str("[13/", con_str(con_str(con_str(STEPS, "] Output: "), output ? con_str(output_path, name) : "stdout"),
            buf)),
        r_ascii(f, glyph_image.glyphs, glyph_image.width, glyph_image.height));

    clock_gettime(CLOCK_MONOTONIC, &total_end);
    if (visualize_steps) {
        if (!silent)
            fprintf(stderr,
                    "-------------------------------------------------------------------------------------------\n");
        if (!silent)
            fprintf(stderr, "%-80s %7.2lf ms\n", "Total Pipeline Time:",
                    get_time_diff_ms(total_start, total_end));
    }

    fclose(f);
    free(data_dump);
    glyph_image_free(&glyph_image);
    free(tiles);
    free(denoise_radius_vals);
    free(glyphs_storage);

    for (int i = 0; i < buf_cnt; i++) {
        free(str_ptr_buf[i]);
    }

    return 0;
}
