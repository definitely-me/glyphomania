import unicodedata
from PIL import ImageFont, ImageDraw, Image
from PIL.ImageFont import FreeTypeFont as FFF
from collections import deque
from fontTools.ttLib import TTFont
from struct import Struct, pack
from typing import cast


def find_visible(font_ftools: TTFont) -> dict[int, str]:
    cmap: dict[int, str] | None = font_ftools.getBestCmap()
    cmap = dict(sorted(cmap.items()))
    printable_chars: dict[int, str] = {}

    if cmap is None:
        raise Exception
    for i, code in enumerate(cmap):
        char = chr(code)

        category = unicodedata.category(char)

        if char.isprintable() and not category.startswith(("C", "Z")):
            printable_chars[code] = char
        if i > 100000:
            break
    return printable_chars


def get_glyph_bitmap(glyph: str, font: FFF) -> tuple[tuple[tuple[int, ...], ...], int]:
    glyph_width = int(font.getlength(glyph))
    image = Image.new("1", (glyph_width, glyph_height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), glyph, font=font, fill=1)
    flat = cast(tuple[int, ...], image.get_flattened_data())

    if glyph_width <= 8:
        bitmap_2d = tuple(tuple(flat[i * narrow_glyph_width:(i + 1) * narrow_glyph_width]) for i in range(glyph_height))
        return bitmap_2d, 0
    else:
        bitmap_2d = tuple(tuple(flat[i * wide_glyph_width:(i + 1) * wide_glyph_width]) for i in range(glyph_height))
        return bitmap_2d, 1


def generate_dists(bitmap: tuple[tuple[int, ...], ...], part: int) -> list[int]:
    left = part * narrow_glyph_width
    right = (part * narrow_glyph_width) + narrow_glyph_width
    trimmed_bitmap = tuple(row[left:right] for row in bitmap)

    dists: list[int] = []

    for row in range(glyph_height):
        for col in range(narrow_glyph_width):
            dists.append(bfs(trimmed_bitmap, (col, row)))

    return dists


def generate_map(glyphs: dict[int, str], font: FFF) -> list[tuple[int, int, list[int]]]:
    glyph_list: list[tuple[int, bytes, list[int]]] = []

    for code, glyph in glyphs.items():
        map: tuple[tuple[tuple[int, ...], ...], int] = get_glyph_bitmap(glyph, font)
        bitmap: tuple[tuple[int, ...], ...] = map[0]
        mode = map[1]

        if mode == 1:
            for part in range(2):
                dists: list[int] = generate_dists(bitmap, part)
                glyph_list.append((mode, code, dists))

        if mode == 0:
            dists: list[int] = generate_dists(bitmap, mode)
            glyph_list.append((mode, code, dists))

    return glyph_list


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


def generate_rodata(map: list[tuple[int, int, list[int]]], struct: Struct) -> None:
    with open(rodata_path, "wb") as f:
        f.write(pack("<i", len(map)))
        for mode, code, dists in map:
            packed = struct.pack(mode, code, *dists)
            f.write(packed)


if __name__ == "__main__":
    rodata_path = "./resources/glyph_bitmap.bin"
    font_path = "./resources/Unifont.ttf"

    font_size = 16
    narrow_glyph_width = 8
    wide_glyph_width = narrow_glyph_width * 2
    glyph_height = 16

    font_pil: FFF = ImageFont.truetype(font_path, size=16)
    font_ftools: TTFont = TTFont(font_path)

    struct: Struct = Struct(f"<BI{narrow_glyph_width * glyph_height}B")

    visible: dict[int, str] = find_visible(font_ftools)

    map = generate_map(visible, font_pil)

    generate_rodata(map, struct)
