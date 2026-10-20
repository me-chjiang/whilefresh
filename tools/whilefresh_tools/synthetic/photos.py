"""Turn a synthetic thermal receipt rendering into a photographed JPEG.

All transformations are deterministic for a given sample seed; JPEG output can still vary
slightly with Pillow version. The HTML and truth JSON are byte-stable.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

from .model import Sample


CANVAS = (1200, 2050)


def _paper_texture(paper: Image.Image, rng: random.Random, variant: str) -> Image.Image:
    w, h = paper.size
    low = Image.new("L", (32, max(32, round(h * 32 / w))))
    low.putdata([rng.randint(215, 255) for _ in range(low.width * low.height)])
    shade = low.resize((w, h), Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(22))
    if variant == "wrinkled":
        draw = ImageDraw.Draw(shade)
        for _ in range(5):
            y = rng.randint(180, h - 180)
            points = [(x, y + round(13 * math.sin(x / 100 + rng.random()))) for x in range(0, w + 80, 80)]
            draw.line(points, fill=194, width=5)
            draw.line([(x, yy + 9) for x, yy in points], fill=252, width=4)
        shade = shade.filter(ImageFilter.GaussianBlur(6))
    if variant == "dim":
        # An off-axis lamp, darker lower-right corner but text still legible.
        gradient = Image.new("L", (w, h))
        gradient.putdata(
            [min(255, 222 + int(28 * (1 - y / h)) - int(28 * (x / w) * (y / h)))
             for y in range(h) for x in range(w)]
        )
        shade = ImageChops.multiply(shade, gradient)
    rgb = paper.convert("RGB")
    rgb = ImageChops.multiply(rgb, Image.merge("RGB", (shade, shade, shade)))
    return rgb


def _background(size: tuple[int, int], rng: random.Random) -> Image.Image:
    w, h = size
    base = Image.new("RGB", (w, h), (171, 177, 178))
    for i in range(28):
        # Very low-frequency countertop texture, no legible external content.
        x = rng.randint(-300, w)
        y = rng.randint(-250, h)
        r = rng.randint(60, 240)
        v = rng.randint(160, 192)
        ImageDraw.Draw(base, "RGBA").ellipse((x, y, x + r, y + r), fill=(v, v, v, 18))
    return base.filter(ImageFilter.GaussianBlur(5))


def photograph_receipt(source: Path, destination: Path, sample: Sample) -> tuple[int, int]:
    """Render the same receipt under different mild photographic defects."""
    rng = random.Random(sample.seed)
    variant = sample.extra["variant"]
    with Image.open(source) as im:
        im.load()
        paper = im.convert("RGB")
    # Preserve source text; resizing uses a high-quality filter. Mask combines transparent paper
    # and transformed edges, avoiding black corners in the result.
    target_width = 860
    target_height = round(paper.height * target_width / paper.width)
    paper = paper.resize((target_width, target_height), Image.Resampling.LANCZOS)
    paper = _paper_texture(paper, rng, variant)
    mask = Image.new("L", paper.size, 255)
    if variant in ("perspective", "occluded"):
        quad = (
            15, 32,  # upper left in source coordinates
            36, paper.height - 22,  # lower left
            paper.width - 2, paper.height - 8,  # lower right
            paper.width - 25, 5,  # upper right
        )
        paper = paper.transform(paper.size, Image.Transform.QUAD, quad, Image.Resampling.BICUBIC)
        mask = mask.transform(mask.size, Image.Transform.QUAD, quad, Image.Resampling.BICUBIC)
    degrees = {"flat": 0.3, "rotated": -3.3, "perspective": 1.6,
               "wrinkled": -0.8, "occluded": 1.8, "dim": 2.8}[variant]
    paper = paper.rotate(degrees, expand=True, resample=Image.Resampling.BICUBIC, fillcolor=(170, 176, 176))
    mask = mask.rotate(degrees, expand=True, resample=Image.Resampling.BICUBIC, fillcolor=0)
    if variant in ("rotated", "perspective", "dim"):
        paper = paper.filter(ImageFilter.GaussianBlur({"rotated": 0.35, "perspective": 0.7, "dim": 0.4}[variant]))
    if variant == "dim":
        paper = ImageEnhance.Contrast(paper).enhance(0.78)
    height = max(CANVAS[1], paper.height + 180)
    canvas = _background((CANVAS[0], height), rng)
    x = (CANVAS[0] - paper.width) // 2 + rng.randint(-18, 18)
    y = 55
    shadow = Image.new("L", paper.size, 0)
    shadow.paste(mask)
    shadow = shadow.filter(ImageFilter.GaussianBlur(22))
    canvas.paste((85, 90, 89), (x + 20, y + 24, x + 20 + paper.width, y + 24 + paper.height), shadow)
    canvas.paste(paper, (x, y), mask)
    if variant == "occluded":
        # A plain card covers two near-bottom lines (marked optional in the gold truth).
        draw = ImageDraw.Draw(canvas)
        card_y = y + int(paper.height * 0.595)
        draw.rounded_rectangle((x - 60, card_y, x + paper.width * 0.82, card_y + 175),
                               radius=12, fill=(144, 166, 181), outline=(125, 147, 162), width=3)
    canvas.save(destination, quality=90, subsampling=0, optimize=True)
    return canvas.size
