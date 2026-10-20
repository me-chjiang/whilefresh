"""One seamless 160px tile: warm transparent grain over a 20px dotted grid."""
import random
from pathlib import Path

from PIL import Image


def build(out_dir: Path) -> None:
    rng = random.Random(8244)
    image = Image.new("RGBA", (160, 160))
    pixels = image.load()
    for y in range(160):
        for x in range(160):
            grain = rng.randrange(0, 23)
            dot = 68 if x % 20 in (9, 10) and y % 20 in (9, 10) else 0
            pixels[x, y] = (140, 91, 41, max(grain, dot))
    out_dir.mkdir(parents=True, exist_ok=True)
    image.save(out_dir / "paper-tile.png", optimize=True)
    stripes = Image.new("RGBA", (10, 10))
    stripe_pixels = stripes.load()
    for y in range(10):
        for x in range(10):
            stripe_pixels[x, y] = (255, 255, 255, 96 if (x + y) % 10 < 5 else 0)
    stripes.save(out_dir / "tape-stripe.png", optimize=True)
