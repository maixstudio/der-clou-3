"""Prepare the inventory icons for the game.

Blender renders one picture per item (Art/Items/build_items.py --icons -> Saved/ItemIcons/raw/<id>.png,
transparent). This script fits each one to the item's grid footprint from the catalog
(Source/DerClou3/Private/DCItemTypes.cpp, e.g. crowbar 1 x 4, laptop 3 x 2): crop to the visible pixels,
turn it a quarter if it lies across the footprint, centre it with a small margin and write
Content/Slate/ItemIcons/<id>.png (ICON_CELL pixels per cell; items without a footprint get a square).

    python Scripts/Tools/prepare_item_icons.py --ids     # print the catalog ids (for build_items.py --items)
    python Scripts/Tools/prepare_item_icons.py           # all rendered icons
    python Scripts/Tools/prepare_item_icons.py ring cash # only these

Whole pipeline:
    blender -b --factory-startup -P Art/Items/build_items.py -- --icons --items <ids>
    python Scripts/Tools/prepare_item_icons.py

Needs Pillow.
"""

import re
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "Source" / "DerClou3" / "Private" / "DCItemTypes.cpp"
RAW = ROOT / "Saved" / "ItemIcons" / "raw"
TARGET = ROOT / "Content" / "Slate" / "ItemIcons"
# Grid cells are 36 units on screen; 64 pixels per cell stays sharp up to about 1.8x UI scale.
ICON_CELL = 64
# Items without a footprint (furniture, paintings): square, as big as two cells.
SQUARE = 128
MARGIN = 0.08
# Only clearly long things (crowbar, torch) are turned a quarter to fit their footprint; folded clothes stay
# upright with the collar on top and simply get some empty space.
TURN_RATIO = 1.6

DEF_PATTERN = re.compile(
    r'FDef\(TEXT\("(?P<id>\w+)"\),\s*LOCTEXT\([^)]*\),\s*\w+,\s*[\d.]+f,\s*(?P<cells>\{\s*\d+\s*,\s*\d+\s*\}|None)')


def catalog() -> dict[str, tuple[int, int] | None]:
    found: dict[str, tuple[int, int] | None] = {}
    for match in DEF_PATTERN.finditer(CATALOG.read_text(encoding="utf-8")):
        cells = match.group("cells")
        if cells == "None":
            found[match.group("id")] = None
        else:
            w, h = (int(v) for v in re.findall(r"\d+", cells))
            found[match.group("id")] = (w, h) if w > 0 and h > 0 else None
    return found


def fit(raw: Image.Image, footprint: tuple[int, int] | None) -> Image.Image:
    box = raw.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    picture = raw.crop(box) if box else raw
    if footprint:
        fw, fh = footprint
        size = (fw * ICON_CELL, fh * ICON_CELL)
        lies_across = (fh > fw and picture.width > picture.height * TURN_RATIO) or \
                      (fw > fh and picture.height > picture.width * TURN_RATIO)
        if lies_across:
            # Counter-clockwise: the right end (handles, hooks) ends up on top.
            picture = picture.rotate(90, expand=True)
    else:
        size = (SQUARE, SQUARE)
    room = (size[0] * (1 - 2 * MARGIN), size[1] * (1 - 2 * MARGIN))
    scale = min(room[0] / picture.width, room[1] / picture.height)
    picture = picture.resize((max(1, round(picture.width * scale)), max(1, round(picture.height * scale))), Image.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(picture, ((size[0] - picture.width) // 2, (size[1] - picture.height) // 2), picture)
    return canvas


def main() -> int:
    items = catalog()
    if "--ids" in sys.argv:
        print(",".join(items))
        return 0
    wanted = {name for name in sys.argv[1:] if not name.startswith("-")}
    TARGET.mkdir(parents=True, exist_ok=True)
    done = 0
    for item_id, footprint in items.items():
        if wanted and item_id not in wanted:
            continue
        raw_path = RAW / f"{item_id}.png"
        if not raw_path.exists():
            print(f"{item_id}: no render in {RAW.relative_to(ROOT)}")
            continue
        icon = fit(Image.open(raw_path).convert("RGBA"), footprint)
        icon.save(TARGET / f"{item_id}.png", optimize=True)
        cells = f"{footprint[0]} x {footprint[1]}" if footprint else "no footprint"
        print(f"{item_id} ({cells}) -> {icon.width} x {icon.height}")
        done += 1
    print(f"{done} icons in {TARGET.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
