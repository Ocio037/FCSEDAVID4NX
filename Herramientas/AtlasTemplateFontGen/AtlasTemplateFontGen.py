from pathlib import Path
import argparse
import math
from PIL import Image, ImageDraw, ImageFont
GRID_COLS = 16
GRID_ROWS = 8
CHAR_SIZE = 32
ATLAS_SIZE = GRID_COLS * CHAR_SIZE
FONT_SIZE = 26
FONT_SCALE = 1.25
CELL_PADDING = 1
GLYPH_WIDTH_SCALE = 1.2
CHARS_PER_LAYER = GRID_COLS * GRID_ROWS
SCRIPT_DIR = Path(__file__).resolve().parent


def get_jis_kanji_range(start_char, end_char):
    chars = []
    found_start = False
    for byte_1 in range(0x88, 0x99):
        for byte_2 in range(0x40, 0xFC):
            if byte_2 == 0x7F:
                continue
            try:
                char = bytes((byte_1, byte_2)).decode("cp932")
            except UnicodeDecodeError:
                continue
            if char == start_char:
                found_start = True
            if found_start:
                chars.append(char)
            if char == end_char:
                return chars
    return chars

def get_korean_ksx1001():
    chars = []
    for byte_1 in range(0xB0, 0xC9):
        for byte_2 in range(0xA1, 0xFF):
            try:
                chars.append(bytes((byte_1, byte_2)).decode("euc-kr"))
            except UnicodeDecodeError:
                continue

    for byte_2 in range(0xA1, 0xD4):
        try:
            chars.append(bytes((0xA4, byte_2)).decode("euc-kr"))
        except UnicodeDecodeError:
            continue
    return chars

def build_character_set(mode):
    if mode == "latin":
        chars = [chr(code) for code in range(0x0020, 0x0100)]
        return chars, ["latin"] * len(chars)
    if mode == "latin-diacritics":
        chars = [chr(code) for code in range(0x0020, 0x007F)] + ["ñ", "Ñ"]
        return chars, ["latin"] * len(chars)

    chars = []
    categories = []
    blocks = [
        (0x0020, 0x00FF, "latin"),  # ASCII y Latin-1
        (0x0370, 0x03FF, "latin"),  # Griego
        (0x0410, 0x044F, "latin"),  # Ruso
        (0x3041, 0x3096, "cjk"),    # Hiragana
        (0x30A1, 0x30FA, "cjk"),    # Katakana
    ]
    for start, end, category in blocks:
        block_chars = [chr(code) for code in range(start, end + 1)]
        chars.extend(block_chars)
        categories.extend([category] * len(block_chars))

    jis_chars = get_jis_kanji_range("亜", "腕")
    chars.extend(jis_chars)
    categories.extend(["cjk"] * len(jis_chars))

    korean_chars = get_korean_ksx1001()
    chars.extend(korean_chars)
    categories.extend(["ko"] * len(korean_chars))
    return chars, categories


def load_font(names, size):
    for name in names:
        local_path = SCRIPT_DIR / name
        try:
            return ImageFont.truetype(str(local_path if local_path.exists() else name), size)
        except (OSError, ValueError):
            continue
    return ImageFont.load_default()


def get_font_metrics(font):
    if hasattr(font, "getmetrics"):
        return font.getmetrics()
    bbox = font.getbbox("Ag")
    return bbox[3] - bbox[1], 0


def draw_glyph_in_cell(dst_img, char, font, cell_x, cell_y, cell_width, cell_height):
    bbox = font.getbbox(char)
    glyph_width = bbox[2] - bbox[0]
    glyph_height = bbox[3] - bbox[1]
    if glyph_width <= 0 or glyph_height <= 0:
        return

    glyph_img = Image.new("L", (glyph_width, glyph_height), 0)
    ImageDraw.Draw(glyph_img).text((-bbox[0], -bbox[1]), char, font=font, fill=255)

    available_width = max(1, cell_width - 1)
    available_height = max(1, cell_height - 2 * CELL_PADDING)
    target_width = min(
        available_width, max(1, round(glyph_img.width * GLYPH_WIDTH_SCALE))
    )
    target_height = min(available_height, glyph_img.height)
    if target_width != glyph_img.width or target_height != glyph_img.height:
        glyph_img = glyph_img.resize(
            (target_width, target_height),
            Image.Resampling.LANCZOS,
        )

    x = cell_x + (cell_width - glyph_img.width) // 2
    y = cell_y + cell_height - CELL_PADDING - glyph_img.height
    dst_img.paste(glyph_img, (x, y))


def parse_args():
    parser = argparse.ArgumentParser(
        description="Genera el atlas de caracteres y font_map.bin. Sin opciones genera el atlas completo."
    )
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument(
        "-a", action="store_true",
        help="genera todos los caracteres del atlas completo.",
    )
    modes.add_argument(
        "-l", action="store_true",
        help="genera ASCII y Latin-1 (U+0020 a U+00FF).",
    )
    modes.add_argument(
        "-ld", action="store_true",
        help="genera ASCII imprimible y añade ñ y Ñ.",
    )
    parser.add_argument(
        "atlas_size", nargs="?", type=int, default=ATLAS_SIZE,
        help=f"ancho y alto de cada capa del atlas en píxeles (predeterminado: {ATLAS_SIZE}).",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    mode = "full" if args.a else "latin" if args.l else "latin-diacritics" if args.ld else "full"
    chars, char_categories = build_character_set(mode)
    if args.atlas_size <= 0 or args.atlas_size % GRID_COLS != 0:
        raise SystemExit(
            f"La resolución del atlas debe ser positiva y divisible entre {GRID_COLS}."
        )

    cell_width = args.atlas_size // GRID_COLS
    cell_height = args.atlas_size // GRID_ROWS
    font_size = max(1, round(cell_height * FONT_SIZE / CHAR_SIZE * FONT_SCALE))

    fonts = {
        "latin": load_font(["arial.ttf"], font_size),
        "cjk": load_font(["msgothic.ttc"], font_size),
        "ko": load_font(["gulim.ttc"], font_size),
    }
    categories_used = set(char_categories)
    for name in ("latin", "cjk", "ko"):
        if name in categories_used:
            ascent, descent = get_font_metrics(fonts[name])
            print(f"{name}: ascent={ascent}, descent={descent}")

    num_layers = max(1, math.ceil(len(chars) / CHARS_PER_LAYER))
    atlas_width = args.atlas_size
    atlas_height = num_layers * args.atlas_size
    atlas_img = Image.new("L", (atlas_width, atlas_height), 0)

    for index, char in enumerate(chars):
        layer = index // CHARS_PER_LAYER
        local_index = index % CHARS_PER_LAYER
        col = local_index % GRID_COLS
        row = local_index // GRID_COLS
        cell_x = col * cell_width
        cell_y = layer * args.atlas_size + row * cell_height
        font = fonts[char_categories[index]]
        draw_glyph_in_cell(
            atlas_img, char, font, cell_x, cell_y, cell_width, cell_height
        )

    atlas_img.convert("RGB").save("AtlasImg_Full.tga")
    with open("font_map.bin", "wb") as font_map:
        font_map.write("".join(chars).encode("utf-16-le"))

    with open("atlas_debug_map.txt", "w", encoding="utf-8") as debug_map:
        debug_map.write("global_index\tlayer\trow\tcol\tchar\tfont_cat\n")
        for index, (char, category) in enumerate(zip(chars, char_categories)):
            layer = index // CHARS_PER_LAYER
            local_index = index % CHARS_PER_LAYER
            row = local_index // GRID_COLS
            col = local_index % GRID_COLS
            debug_map.write(f"{index}\t{layer}\t{row}\t{col}\t{char}\t{category}\n")

    print(
        f"Modo: {mode}; atlas: {args.atlas_size}x{args.atlas_size}px por capa; "
        f"rejilla: {GRID_COLS}x{GRID_ROWS}; celdas: {cell_width}x{cell_height}px; "
        f"fuente: {font_size}px; {CHARS_PER_LAYER} caracteres/capa; {len(chars)} caracteres"
    )
    print(f"Atlas completo: AtlasImg_Full.tga ({atlas_width}x{atlas_height})")
    print(f"font_map.bin generado con {len(chars)} caracteres")


if __name__ == "__main__":
    main()
