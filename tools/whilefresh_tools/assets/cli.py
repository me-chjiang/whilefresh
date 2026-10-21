"""Rebuild the font subset and paper texture for the QML theme."""
import argparse
from pathlib import Path

from . import font, texture

REPO_ROOT = Path(__file__).resolve().parents[3]


def main() -> int:
    parser = argparse.ArgumentParser(prog="whilefresh_tools.assets")
    sub = parser.add_subparsers(dest="command", required=True)
    f = sub.add_parser("font")
    f.add_argument("--source", type=Path, help="local NotoSerifSC-Black.otf")
    f.add_argument("--out", type=Path, default=REPO_ROOT / "app/assets/fonts/NotoSerifSC-Black-subset.otf")
    t = sub.add_parser("texture")
    t.add_argument("--out-dir", type=Path, default=REPO_ROOT / "app/assets/textures")
    args = parser.parse_args()
    if args.command == "font":
        font.build(REPO_ROOT, args.source, args.out)
    else:
        texture.build(args.out_dir)
    return 0
