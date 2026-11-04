from pathlib import Path
import math

from PIL import Image


INPUT_FULL_ATLAS = "AtlasImg_Full.tga"
OUTPUT_DIR = "AtlasChunks"


def slice_full_atlas():
    atlas_path = Path(INPUT_FULL_ATLAS)
    if not atlas_path.exists():
        raise FileNotFoundError(f"No existe el atlas full: {INPUT_FULL_ATLAS}")

    img = Image.open(atlas_path).convert("RGBA")
    width, height = img.size
    page_width = width
    page_height = width
    num_layers = math.ceil(height / page_height)
    out_dir = Path(OUTPUT_DIR)
    out_dir.mkdir(exist_ok=True)

    for layer in range(num_layers):
        src_y = layer * page_height
        chunk_height = min(page_height, max(0, height - src_y))
        layer_img = Image.new("RGBA", (page_width, page_height), (0, 0, 0, 0))
        if chunk_height > 0:
            crop = img.crop((0, src_y, page_width, src_y + chunk_height))
            layer_img.paste(crop, (0, 0), crop)
        layer_img.save(out_dir / f"AtlasLayer_{layer:03d}.tga")

    print(f"Capas generadas: {num_layers}")
    print(f"Salida: {out_dir.resolve()}")


if __name__ == "__main__":
    slice_full_atlas()
