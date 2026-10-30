from collections import deque
from typing import cast

from PIL import ImageFont, ImageDraw, Image

h_file_path = "/glyph_bitmap.h"
font_path = "/usr/share/fonts/Unifont/Unifont.ttf"
font_size = 16
narrow_glyph_width = 8
wide_glyph_width = narrow_glyph_width * 2
glyph_height = 16

glyphs = (
    "⎸", "⎹", "⎜", "⎟", "▎",

    "▔", "¯", "⎺", "⎻", "—", "─", "━", "⎼", "⎽", "_", "˭",

    "⌜", "⌝", "⌞", "⌟", "┚", "┖", "┍", "⨽", "⨼",

    "/", "\\", "╱", "╲", "⊿", "⊾", "⦦", "⦧",

    "╸", "╺",

    "‥", "‶", "″", "､",

    "˘", "⁀", "‿",

    "ｰ", "｢", "｣", "ﾇ", "ﾆ", "ﾍ", "ｮ", "ｲ", "ﾉ",

    "⎩", "⎛", "⎫", "⎞", "⎠", "⎝", "⌠", "⌡",
)

font = ImageFont.truetype(font_path, size=16)


def get_glyph_bitmap(glyph: str) -> tuple[tuple[tuple[int, ...], ...], int]:
    glyph_width = font.getlength(glyph)

    if glyph_width <= 8:
        image = Image.new("1", (narrow_glyph_width, glyph_height), 0)
        draw = ImageDraw.Draw(image)
        draw.text((0, 0), glyph, font=font, fill=1)
        flat = cast(tuple[int, ...], image.get_flattened_data())
        bitmap_2d = tuple(tuple(flat[i * narrow_glyph_width:(i + 1) * narrow_glyph_width]) for i in range(glyph_height))
        return bitmap_2d, 0
    else:
        image = Image.new("1", (wide_glyph_width, glyph_height), 0)
        draw = ImageDraw.Draw(image)
        draw.text((0, 0), glyph, font=font, fill=1)
        flat = cast(tuple[int, ...], image.get_flattened_data())
        bitmap_2d = tuple(tuple(flat[i * wide_glyph_width:(i + 1) * wide_glyph_width]) for i in range(glyph_height))
        return bitmap_2d, 1


def generate_map(glyphs: tuple[str, ...]) -> dict[str, tuple[tuple[tuple[int, ...], ...], int]]:
    glyph_map = {}
    for glyph in glyphs:
        glyph_map[glyph] = get_glyph_bitmap(glyph)
    return glyph_map


def bfs(bitmap: tuple[tuple[int, ...], ...], start: tuple[int, int]) -> int:
    rows = len(bitmap)
    cols = len(bitmap[0])

    queue = deque([(start[0], start[1], 0)])
    visited = {start}

    directions = [(-1, 0), (1, 0), (0, -1), (0, 1)]

    while queue:
        x, y, dist = queue.popleft()

        if bitmap[y][x] == 1:
            return dist

        for dx, dy in directions:
            nx, ny = x + dx, y + dy

            if 0 <= nx < cols and 0 <= ny < rows and (nx, ny) not in visited:
                visited.add((nx, ny))
                queue.append((nx, ny, dist + 1))

    return 255


def c_string_literal(glyph: str) -> str:
    return glyph.replace("\\", "\\\\").replace('"', '\\"')


def generate_header(h_path: str, glyph_map: dict[str, tuple[tuple[tuple[int, ...], ...], int]]) -> None:
    wide_glyphs: list[tuple[str, tuple[tuple[int, ...], ...]]] = []

    wide_count = sum(1 for _, (_, is_wide) in glyph_map.items() if is_wide)
    narrow_count = len(glyph_map) - wide_count
    total_tiles = narrow_count + wide_count * 2

    with open(h_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write("typedef struct {\n")
        f.write("    const char *glyph;\n")
        f.write(f"    uint8_t bitmap[{narrow_glyph_width * glyph_height}];\n")
        f.write("} GlyphTile;\n\n")
        f.write(f"#define GLYPH_TILE_COUNT {total_tiles}\n")
        f.write(f"#define GLYPH_WIDE_OFFSET {narrow_count}\n")
        f.write(f"#define PREFERRED_WIDTH_DIVISOR {narrow_glyph_width}\n")
        f.write(f"#define PREFERRED_HEIGHT_DIVISOR {glyph_height}\n\n")
        f.write(f"static const GlyphTile GLYPHS[{total_tiles}] = {{\n")

        for glyph, (bitmap_2d, is_wide) in glyph_map.items():
            if is_wide:
                wide_glyphs.append((glyph, bitmap_2d))
                continue

            f.write("    {\n")
            f.write(f'        .glyph = "{c_string_literal(glyph)}",\n')
            f.write("        .bitmap = {\n")
            f.write("            ")
            dists_2d = []
            for y in range(glyph_height):
                row_dists = []
                for x in range(narrow_glyph_width):
                    row_dists.append(bfs(bitmap_2d, (x, y)))
                dists_2d.append(row_dists)
            f.write(", ".join(str(value) for row in dists_2d for value in row))
            f.write("\n        },\n")
            f.write("    },\n")

        for glyph, bitmap_2d in wide_glyphs:
            left_half = tuple(row[:narrow_glyph_width] for row in bitmap_2d)
            right_half = tuple(row[narrow_glyph_width:] for row in bitmap_2d)

            for half in (left_half, right_half):
                f.write("    {\n")
                f.write(f'        .glyph = "{c_string_literal(glyph)}",\n')
                f.write("        .bitmap = {\n")
                f.write("            ")

                dists_2d = []
                for y in range(glyph_height):
                    row_dists = []
                    for x in range(narrow_glyph_width):
                        row_dists.append(bfs(half, (x, y)))
                    dists_2d.append(row_dists)

                flat_dists = [val for row in dists_2d for val in row]

                f.write(", ".join(str(value) for value in flat_dists))
                f.write("\n        },\n")
                f.write("    },\n")

        f.write("};\n")


if __name__ == "__main__":
    generate_header(h_file_path, generate_map(glyphs))
