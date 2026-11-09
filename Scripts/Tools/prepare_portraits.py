"""Prepare crew portraits for the game.

Reads the originals from Art/Portraits (png, jpg, webp; file name = crew name, e.g. vic.png),
cuts them to 3:4 (centre for wide pictures, upper part for tall ones, like DCItemWidgets::Portrait),
moves a little closer to the face (ZOOM) and writes small PNGs to Content/Slate/Portraits,
where the game loads them at runtime.

    python Scripts/Tools/prepare_portraits.py            # all originals
    python Scripts/Tools/prepare_portraits.py vic rosa   # only these

Needs Pillow.
"""

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Art" / "Portraits"
TARGET = ROOT / "Content" / "Slate" / "Portraits"
# Twice the card size (66 x 88) at 1080p: sharp up to 4K, without much aliasing when Slate scales it down.
SIZE = (132, 176)
EXTENSIONS = (".png", ".jpg", ".jpeg", ".webp")
# The prompts ask for head and shoulders; on a 66 x 88 card the face reads better a bit closer.
# Keep 75 % of the picture, centred a little right of the middle (where the face is) and near the top (cap, hair).
ZOOM = 0.75
FOCUS_X = 0.53
TOP = 0.03


def cut_to_aspect(picture: Image.Image, aspect: float) -> Image.Image:
    width, height = picture.size
    own = width / height
    if own > aspect:
        keep = round(height * aspect)
        left = (width - keep) // 2
        return picture.crop((left, 0, left + keep, height))
    if own < aspect:
        keep = round(width / aspect)
        top = (height - keep) // 4
        return picture.crop((0, top, width, top + keep))
    return picture


def move_closer(picture: Image.Image) -> Image.Image:
    width, height = picture.size
    keep_w, keep_h = round(width * ZOOM), round(height * ZOOM)
    left = min(max(0, round(width * FOCUS_X - keep_w / 2)), width - keep_w)
    top = min(max(0, round(height * TOP)), height - keep_h)
    return picture.crop((left, top, left + keep_w, top + keep_h))


def main() -> int:
    wanted = {name.lower() for name in sys.argv[1:]}
    originals = [path for path in sorted(SOURCE.glob("*")) if path.suffix.lower() in EXTENSIONS]
    if wanted:
        originals = [path for path in originals if path.stem.lower() in wanted]
    if not originals:
        print(f"No portraits found in {SOURCE}")
        return 1
    TARGET.mkdir(parents=True, exist_ok=True)
    for path in originals:
        picture = Image.open(path).convert("RGB")
        small = move_closer(cut_to_aspect(picture, SIZE[0] / SIZE[1])).resize(SIZE, Image.LANCZOS)
        out = TARGET / f"{path.stem.lower()}.png"
        small.save(out, optimize=True)
        print(f"{path.name} ({picture.width} x {picture.height}) -> {out.relative_to(ROOT)} ({SIZE[0]} x {SIZE[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
