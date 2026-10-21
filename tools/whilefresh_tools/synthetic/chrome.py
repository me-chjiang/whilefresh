"""Render an HTML file to PNG with the locally installed Google Chrome (headless)."""

from __future__ import annotations

import os
import signal
import subprocess
import tempfile
import time
from pathlib import Path

from PIL import Image

from .html_common import SENTINEL_COLOR, SENTINEL_HEIGHT

CHROME = os.environ.get("WHILEFRESH_CHROME", "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
SCALE = 3
MAX_CSS_HEIGHT = 5000  # window height used for rendering; content is cropped at the sentinel strip


class RenderError(RuntimeError):
    pass


def _screenshot(html: Path, png: Path, width: int, height: int, scale: int, timeout: float = 40.0) -> None:
    """Headless Chrome writes the file but may linger afterwards, so wait for the file and kill it."""
    with tempfile.TemporaryDirectory(prefix="whilefresh-chrome-") as profile:
        if png.exists():
            png.unlink()
        cmd = [
            CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
            "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
            "--disable-component-update", "--disable-extensions",
            f"--user-data-dir={profile}", f"--force-device-scale-factor={scale}",
            f"--window-size={width},{height}", f"--screenshot={png}", f"file://{html}",
        ]
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        try:
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if png.exists():
                    time.sleep(0.4)  # let the write finish
                    return
                if proc.poll() is not None:
                    break
                time.sleep(0.1)
            if not png.exists():
                raise RenderError(f"Chrome produced no screenshot for {html}")
        finally:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()


def _sentinel_row(img: Image.Image, scale: int) -> int:
    rgb = img.convert("RGB")
    x = min(5, rgb.width - 1)
    for y in range(rgb.height - 1, -1, -1):
        if rgb.getpixel((x, y)) == SENTINEL_COLOR:
            # scan upward to the first row of the strip
            top = y
            while top > 0 and rgb.getpixel((x, top - 1)) == SENTINEL_COLOR:
                top -= 1
            if y - top + 1 >= SENTINEL_HEIGHT * scale - 1:
                return top
    raise RenderError("sentinel strip not found: page taller than the render window?")


def render_page(html: Path, png: Path, css_width: int, scale: int = SCALE) -> tuple[int, int]:
    """Render `html` and crop it to its content. Returns the final pixel size."""
    tmp = png.with_suffix(".full.png")
    _screenshot(html, tmp, css_width, MAX_CSS_HEIGHT, scale)
    try:
        with Image.open(tmp) as img:
            img.load()
            cut = _sentinel_row(img, scale)
            cropped = img.convert("RGB").crop((0, 0, img.width, cut))
    finally:
        tmp.unlink(missing_ok=True)
    cropped.save(png, optimize=True)
    return cropped.size
