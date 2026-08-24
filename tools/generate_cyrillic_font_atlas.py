#!/usr/bin/env python3
"""Generate UT99 Android's CP1251 Cyrillic fallback glyph atlas.

The generated file contains only rasterized glyph pixels and compact metrics;
it does not embed or redistribute the source TTF/OTF files.

Requires Pillow. By default this tool uses Noto Sans Regular/Bold from common
Linux locations. Alternative OFL-compatible source fonts may be supplied with
--regular and --bold.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

MAGIC = b"UTRUCP1\0"
FONT_SIZE = 22
CELL_SIZE = 32
COLUMNS = 16
ROWS_PER_FACE = 8
FIRST_BYTE = 0x80
LAST_BYTE = 0xFF


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--regular",
        type=Path,
        default=Path("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"),
        help="Regular TrueType/OpenType font with CP1251 Cyrillic coverage.",
    )
    parser.add_argument(
        "--bold",
        type=Path,
        default=Path("/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"),
        help="Bold TrueType/OpenType font with CP1251 Cyrillic coverage.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Destination CyrillicFontAtlas.dat path.",
    )
    return parser.parse_args()


def cp1251_character(byte_value: int) -> str:
    try:
        return bytes((byte_value,)).decode("cp1251")
    except UnicodeDecodeError:
        # Windows-1251 has one undefined slot (0x98). It is deliberately made
        # visible as '?' instead of creating a zero-sized record.
        return "?"


def render_face(
    image: Image.Image,
    font: ImageFont.FreeTypeFont,
    face_index: int,
) -> list[tuple[int, int, int, int, int]]:
    draw = ImageDraw.Draw(image)
    records: list[tuple[int, int, int, int, int]] = []
    face_y = face_index * ROWS_PER_FACE * CELL_SIZE

    for index, byte_value in enumerate(range(FIRST_BYTE, LAST_BYTE + 1)):
        row = index // COLUMNS
        column = index % COLUMNS
        x = column * CELL_SIZE
        y = face_y + row * CELL_SIZE
        character = cp1251_character(byte_value)
        advance = max(1, min(CELL_SIZE, int(math.ceil(font.getlength(character)))))

        # A fixed line box matches classic UE1 UFont behavior better than
        # tightly cropping each glyph: VSize remains stable for a given face.
        draw.text((x, y + 25), character, font=font, fill=255, anchor="ls")
        records.append((x, y, advance, CELL_SIZE, advance))

    return records


def build_atlas(regular_path: Path, bold_path: Path) -> bytes:
    for path in (regular_path, bold_path):
        if not path.is_file():
            raise FileNotFoundError(path)

    width = COLUMNS * CELL_SIZE
    height = ROWS_PER_FACE * 2 * CELL_SIZE
    image = Image.new("L", (width, height), 0)
    fonts = (
        ImageFont.truetype(str(regular_path), FONT_SIZE),
        ImageFont.truetype(str(bold_path), FONT_SIZE),
    )

    records: list[tuple[int, int, int, int, int]] = []
    for face_index, font in enumerate(fonts):
        records.extend(render_face(image, font, face_index))

    if len(records) != 256:
        raise RuntimeError(f"expected 256 glyph records, got {len(records)}")

    data = bytearray(MAGIC)
    data += struct.pack("<HHHH", width, height, CELL_SIZE, len(records))
    for record in records:
        data += struct.pack("<HHHHH", *record)
    data += image.tobytes()
    return bytes(data)


def main() -> int:
    args = parse_args()
    payload = build_atlas(args.regular, args.bold)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    digest = hashlib.sha256(payload).hexdigest()
    print(f"{args.output}: {len(payload)} bytes")
    print(f"SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
