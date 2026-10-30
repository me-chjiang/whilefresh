import hashlib
import io
import re
import sys
import urllib.request
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

SOURCE_URL = "https://github.com/notofonts/noto-cjk/raw/Serif2.003/Serif/SubsetOTF/SC/NotoSerifSC-Black.otf"
SOURCE_SHA256 = "91dde57622845beeb663f300813999febdb7fcd5e451bde199d7c954a9740cb4"
EXTRA_TEXT = "０１２３４５６７８９，。、；：？！“”‘’（）《》【】—…·～＋－×÷"
ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
CJK_RE = re.compile(r"[　-〿一-鿿＀-￯‐-‧]")


def gb2312_level1() -> str:
    chars = []
    for row in range(0xB0, 0xD8):
        for col in range(0xA1, 0xFF):
            try:
                chars.append(bytes([row, col]).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return "".join(chars)


def build(repo_root: Path, source: Path | None, out: Path) -> None:
    if source:
        data = source.read_bytes()
    else:
        print(f"downloading {SOURCE_URL}", file=sys.stderr)
        with urllib.request.urlopen(SOURCE_URL, timeout=300) as response:
            data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_SHA256:
        raise SystemExit(f"source font sha256 mismatch: {digest}")
    qml = "".join(path.read_text(encoding="utf-8") for path in (repo_root / "app/qml").rglob("*.qml"))
    text = ASCII + EXTRA_TEXT + gb2312_level1() + "".join(CJK_RE.findall(qml))
    options = subset.Options()
    options.name_IDs = ["*"]
    options.hinting = False
    face = TTFont(io.BytesIO(data))
    subsetter = subset.Subsetter(options)
    subsetter.populate(text=text)
    subsetter.subset(face)
    out.parent.mkdir(parents=True, exist_ok=True)
    face.save(out)
    print(f"{out}: {out.stat().st_size / 1024:.0f} KiB; {len(face.getGlyphOrder())} glyphs")
