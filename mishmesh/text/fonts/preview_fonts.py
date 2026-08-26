#!/usr/bin/env python3
"""Render generated Latin or Cyrillic C atlases exactly as the OLED sees them."""

import argparse
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


CYRILLIC_RANGES = ((0x0400, 0x045F), (0x0490, 0x0491))
UPPER = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯЂЃЄЅІЇЈЉЊЋЌЎЏҐ"
LOWER = "абвгдеёжзийклмнопрстуфхцчшщъыьэюяђѓєѕіїјљњћќўџґ"
ROLE_HEIGHTS = {"Body": 10, "Subtitle": 15, "Caption": 6}
ROLE_ASCII_HEIGHTS = {"Body": 8, "Subtitle": 13, "Caption": 6}
ROLE_LINE_HEIGHTS = {"Body": 11, "Subtitle": 17, "Caption": 6}
ROLE_OFFSETS = {
    "Body": {"latin": 1, "cyrillic": 0},
    "Subtitle": {"latin": 1, "cyrillic": 0},
    "Caption": {"latin": 0, "cyrillic": 0},
}
ROLE_SCALES = {"Body": 6, "Subtitle": 4, "Caption": 9}
ALPHABETS = {
    "cyrillic": (UPPER, LOWER),
    "latin": ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"),
}
COLS = 11
CELL_WIDTH = 84
CELL_HEIGHT = 88
MARGIN = 24


def parse_array(source, name):
    match = re.search(rf"{re.escape(name)}\[[^]]+\].*?=\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise RuntimeError(f"cannot find {name}")
    return [int(value, 0) for value in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", match.group(1))]


def load_atlas(path, role, charset):
    source = path.read_text(encoding="ascii")
    prefix = f"mf_bwfont_{role}"
    stored_height = ROLE_HEIGHTS[role] if charset == "cyrillic" else ROLE_ASCII_HEIGHTS[role]
    line_height = ROLE_LINE_HEIGHTS[role]
    offset_y = ROLE_OFFSETS[role][charset]
    height_bytes = (stored_height + 7) // 8

    # Cyrillic lives in one array per contiguous range; Latin is a single block.
    banks = []
    if charset == "cyrillic":
        spans = CYRILLIC_RANGES
        suffixes = [f"cyr{i}" for i in range(len(spans))]
    else:
        spans = ((0x20, 0x7E),)
        suffixes = ["0"]
    for (lo, _hi), suffix in zip(spans, suffixes):
        banks.append((
            lo,
            parse_array(source, f"{prefix}_glyph_data_{suffix}"),
            parse_array(source, f"{prefix}_glyph_offsets_{suffix}"),
            parse_array(source, f"{prefix}_glyph_widths_{suffix}"),
        ))

    def bank_for(char):
        for lo, data, offsets, advances in banks:
            index = ord(char) - lo
            if 0 <= index < len(advances):
                return lo, data, offsets, advances
        raise KeyError(char)

    def glyph(char):
        first, data, offsets, advances = bank_for(char)
        index = ord(char) - first
        first_column, last_column = offsets[index:index + 2]
        columns = []
        for column in range(first_column, last_column):
            base = column * height_bytes
            pixels = []
            for y in range(line_height):
                source_y = y - offset_y
                pixels.append(
                    0 <= source_y < stored_height and
                    bool(data[base + source_y // 8] & (1 << (source_y % 8)))
                )
            columns.append(pixels)
        return columns, advances[index]

    return glyph


def label_font(size):
    return ImageFont.load_default(size=size)


def draw_glyph(draw, origin, columns, advance, height, scale):
    x0, y0 = origin
    grid_width = max(advance, len(columns)) * scale
    draw.rectangle((x0 - 1, y0 - 1, x0 + grid_width, y0 + height * scale), outline="#26384a")
    draw.line((x0 + advance * scale, y0, x0 + advance * scale, y0 + height * scale - 1), fill="#18a9b8")
    for x, column in enumerate(columns):
        for y, enabled in enumerate(column):
            if enabled:
                draw.rectangle(
                    (x0 + x * scale, y0 + y * scale,
                     x0 + (x + 1) * scale - 1, y0 + (y + 1) * scale - 1),
                    fill="#f4f8fb",
                )


def render(fonts_dir, output, roles, charset):
    title_height = 68
    alphabets = ALPHABETS[charset]
    rows_per_case = max(-(-len(a) // COLS) for a in alphabets)
    section_height = title_height + rows_per_case * len(alphabets) * CELL_HEIGHT
    width = MARGIN * 2 + COLS * CELL_WIDTH
    height = MARGIN * 2 + len(roles) * section_height
    image = Image.new("RGB", (width, height), "#071019")
    draw = ImageDraw.Draw(image)
    title = label_font(24)
    label = label_font(14)
    small = label_font(11)

    draw.text((MARGIN, 10), f"Generated {charset.title()} atlas preview", font=title, fill="#f4f8fb")
    draw.text((MARGIN, 36), "White: OLED pixels   Cyan: advance boundary", font=small, fill="#7f9bad")

    section_y = MARGIN + 38
    for role in roles:
        source = fonts_dir / f"{role}.c"
        glyph = load_atlas(source, role, charset)
        scale = ROLE_SCALES[role]
        glyph_height = ROLE_LINE_HEIGHTS[role]
        draw.text((MARGIN, section_y), f"{role}  ({glyph_height}px)", font=title, fill="#59dbe7")
        draw.text((MARGIN + 220, section_y + 7), source.name, font=small, fill="#7f9bad")

        grid_y = section_y + 38
        for case_index, alphabet in enumerate(alphabets):
            for index, char in enumerate(alphabet):
                row = case_index * rows_per_case + index // COLS
                column = index % COLS
                cell_x = MARGIN + column * CELL_WIDTH
                cell_y = grid_y + row * CELL_HEIGHT
                columns, advance = glyph(char)
                draw.text((cell_x + 2, cell_y), f"{char} {ord(char):04X}", font=label, fill="#a8bac7")
                draw_glyph(
                    draw,
                    (cell_x + 8, cell_y + 22),
                    columns,
                    advance,
                    glyph_height,
                    scale,
                )

        section_y += section_height

    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    print(f"wrote {output.resolve()} ({image.width}x{image.height})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, help="output PNG path")
    parser.add_argument(
        "--roles", nargs="+", choices=tuple(ROLE_HEIGHTS),
        default=list(ROLE_HEIGHTS), help="font roles to include",
    )
    parser.add_argument(
        "--charset", choices=tuple(ALPHABETS), default="cyrillic",
        help="character set to preview",
    )
    args = parser.parse_args()
    fonts_dir = Path(__file__).resolve().parent
    output = args.output or fonts_dir / f"{args.charset}_preview.png"
    render(fonts_dir, output, args.roles, args.charset)


if __name__ == "__main__":
    main()
