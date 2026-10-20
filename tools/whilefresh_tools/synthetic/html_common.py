"""Shared HTML helpers for the screenshot and receipt renderers."""

from __future__ import annotations

from html import escape

from .model import Product

SENTINEL_COLOR = (254, 0, 254)
SENTINEL_HEIGHT = 3  # CSS px; the renderer crops the screenshot above this strip

BASE_CSS = """
*{box-sizing:border-box;margin:0;padding:0}
html,body{background:#fff}
body{font-family:-apple-system,"PingFang SC","Helvetica Neue",Arial,sans-serif;color:#222;
  -webkit-font-smoothing:antialiased;line-height:1.3}
.thumb{display:flex;align-items:center;justify-content:center;border-radius:6px;flex:none}
.ellipsis2{display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.row{display:flex}
.grow{flex:1;min-width:0}
"""

_EMOJI = {
    "鸡蛋": "🥚", "鸭蛋": "🥚", "西红柿": "🍅", "圣女果": "🍅", "黄瓜": "🥒", "肥牛": "🥩", "牛里脊": "🥩",
    "牛腩": "🥩", "牛排": "🥩", "牛腱": "🥩", "羊肉卷": "🥩", "五花肉": "🥓", "猪肉": "🥓", "猪肉末": "🥩",
    "排骨": "🍖", "猪蹄": "🍖", "鸡腿": "🍗", "鸡翅": "🍗", "鸡胸肉": "🍗", "鸡爪": "🍗", "鸭腿": "🍗",
    "上海青": "🥬", "小白菜": "🥬", "油麦菜": "🥬", "大白菜": "🥬", "生菜": "🥬", "菠菜": "🥬", "茼蒿": "🥬",
    "芥蓝": "🥬", "芹菜": "🥬", "香菜": "🌿", "小葱": "🌿", "葱": "🌿", "牛奶": "🥛", "酸奶": "🥛",
    "豆腐": "⬜", "豆腐干": "🟫", "豆皮": "🟨", "草莓": "🍓", "洋葱": "🧅", "虾仁": "🦐", "白虾": "🦐",
    "基围虾": "🦐", "三文鱼": "🐟", "鲈鱼": "🐟", "带鱼": "🐟", "鲫鱼": "🐟", "鳕鱼": "🐟", "鱿鱼": "🦑",
    "螃蟹": "🦀", "蛤蜊": "🐚", "柠檬": "🍋", "苹果": "🍎", "吐司": "🍞", "面包": "🍞", "香蕉": "🍌",
    "橙子": "🍊", "葡萄": "🍇", "牛油果": "🥑", "芒果": "🥭", "榴莲": "🍈", "柚子": "🍊", "土豆": "🥔",
    "红薯": "🍠", "胡萝卜": "🥕", "青椒": "🫑", "西兰花": "🥦", "金针菇": "🍄", "香菇": "🍄", "干香菇": "🍄",
    "木耳": "🍄", "玉米": "🌽", "秋葵": "🥒", "茄子": "🍆", "南瓜": "🎃", "冬瓜": "🥒", "莲藕": "🥔",
    "山药": "🥔", "大米": "🌾", "面粉": "🌾", "面条": "🍜", "意面": "🍝", "粉丝": "🍜", "食用油": "🛢️",
    "火腿": "🥓", "豆芽": "🌱", "四季豆": "🫛", "荷兰豆": "🫛", "紫甘蓝": "🥬", "海带": "🌿", "紫菜": "🌿",
    "姜": "🫚", "蒜": "🧄", "花生": "🥜", "核桃": "🥜", "腰果": "🥜", "枣": "🟤", "枸杞": "🔴",
    "芝士片": "🧀", "鸡蛋饼": "🥞", "腐竹": "🟨", "韭菜": "🌿",
}


def emoji_for(p: Product) -> str:
    if p.emoji:
        return p.emoji
    if p.category == "seasoning":
        return "🧂"
    if p.category == "irrelevant":
        return "🧴"
    return _EMOJI.get(p.visual_key or p.canonical, "🥬")


# Soft pastel backgrounds for product thumbnails, picked by a stable hash of the name.
_PASTELS = ("#fdecec", "#fff4e0", "#eaf6e8", "#e8f1fb", "#f3ecfa", "#fbf1e4", "#e6f5f3", "#f6f0e4")


def pastel(text: str) -> str:
    return _PASTELS[sum(ord(c) for c in text) % len(_PASTELS)]


def esc(text: str | None) -> str:
    return escape(text or "", quote=True)


def money(value: float) -> str:
    return f"{value:.2f}"


def thumb(emoji: str, size: int, key: str, radius: int | None = None) -> str:
    style = f"width:{size}px;height:{size}px;font-size:{int(size * 0.56)}px;background:{pastel(key)}"
    if radius is not None:
        style += f";border-radius:{radius}px"
    return f'<div class="thumb" style="{style}">{emoji}</div>'


def checkbox(on: bool, color: str, size: int = 18) -> str:
    if on:
        style = f"width:{size}px;height:{size}px;border-radius:50%;background:{color};color:#fff;font-size:{size - 5}px;"
        return f'<div style="{style}display:flex;align-items:center;justify-content:center;flex:none">✓</div>'
    style = f"width:{size}px;height:{size}px;border-radius:50%;border:1.5px solid #bbb;flex:none"
    return f'<div style="{style}"></div>'


def document(body: str, css: str, width: int, min_height: int, title: str) -> str:
    return (
        "<!doctype html>\n"
        '<html lang="zh-CN"><head><meta charset="utf-8">'
        f"<title>{esc(title)}</title>"
        f"<style>{BASE_CSS}{css}</style></head>\n"
        f'<body><div class="screen" style="width:{width}px;min-height:{min_height}px">\n{body}\n</div>'
        f'<div id="end" style="height:{SENTINEL_HEIGHT}px;background:rgb{SENTINEL_COLOR}"></div></body></html>\n'
    )
