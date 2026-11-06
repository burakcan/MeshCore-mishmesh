#!/usr/bin/env python3
"""Convert the custom sleep image PNG to the firmware bitmap. Requires Pillow."""

from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "mishmesh/assets/custom_sleep_image.png"
DESTINATION = ROOT / "mishmesh/core/custom_sleep_image.h"

def main():
    with Image.open(SOURCE) as source:
        if source.size != (122, 250):
            raise ValueError("mishmesh/assets/custom_sleep_image.png must be 122 x 250 pixels (portrait)")
        # Composite transparency onto white, then threshold without ditherin
        rgba = source.convert("RGBA")
        white = Image.new("RGBA", rgba.size, "white")
        gray = Image.alpha_composite(white, rgba).convert("L")
        stride = (gray.width + 7) // 8
        bits = bytearray(stride * gray.height)
        for y in range(gray.height):
            for x in range(gray.width):
                # The driver paints set bits in the foreground colour (black on e-ink), MSB first, with each row padded to whole bytes
                if gray.getpixel((x, y)) < 128:
                    bits[y * stride + x // 8] |= 0x80 >> (x % 8)

    rows = [
        "    " + ", ".join(f"0x{value:02x}" for value in bits[i:i + stride]) + ","
        for i in range(0, len(bits), stride)
    ]
    DESTINATION.write_text(
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        "namespace mishmesh {\n\n"
        "// Generated from mishmesh/assets/custom_sleep_image.png by mishmesh/tools/generate_sleep_image.py.\n"
        "static const int CUSTOM_SLEEP_IMAGE_W = 122;\n"
        "static const int CUSTOM_SLEEP_IMAGE_H = 250;\n"
        "static const uint8_t CUSTOM_SLEEP_IMAGE[] = {\n"
        + "\n".join(rows)
        + "\n};\n\n}  // namespace mishmesh\n"
    )
    print(f"Generated {DESTINATION.relative_to(ROOT)} ({len(bits)} bitmap bytes)")


if __name__ == "__main__":
    main()
