"""Parse one HowToCook recipe Markdown file into a faithful structured record.

No ingredient normalisation happens here (that is the vocabulary task); the
only judgement calls are the optional flag and the ingredient/tool kind.
"""

import re
import unicodedata
from urllib.parse import quote

from .fetch import COMMIT, REPO_URL

CATEGORY_ZH = {
    "aquatic": "水产",
    "breakfast": "早餐",
    "condiment": "调料",
    "dessert": "甜品",
    "drink": "饮品",
    "meat_dish": "荤菜",
    "semi-finished": "半成品",
    "soup": "汤与粥",
    "staple": "主食",
    "vegetable_dish": "素菜",
}

# Multi-character terms that are unambiguous wherever they appear in a name.
TOOL_CONTAINS = (
    "烤箱", "微波炉", "电饭煲", "电饭锅", "空气炸锅", "蒸笼", "蒸架", "烤架", "烤网",
    "烤盘", "砧板", "案板", "搅拌机", "料理机", "破壁机", "榨汁机", "打蛋器", "搅拌器",
    "平底锅", "炒锅", "汤锅", "煮锅", "砂锅", "高压锅", "压力锅", "不粘锅", "蒸锅",
    "炖锅", "电磁炉", "燃气灶", "擀面杖", "菜刀", "刮刀", "厨房纸", "保鲜膜", "锡纸",
    "烘焙纸", "油纸", "牙签", "温度计", "电子秤", "厨房秤", "量杯", "量勺", "漏勺",
    "滤网", "筛子", "打火机", "吧勺", "摇酒壶", "雪克杯", "裱花", "模具", "保鲜盒",
    "保鲜袋", "剪刀", "锅铲", "汤勺", "饭勺", "筷子", "刨丝器", "削皮刀", "削皮器",
    "夹子", "吸管", "手套", "刷子", "铲子", "秒表", "笊篱", "蒸篦", "蒜臼", "研杵", "电饼铛",
    "簸箕", "盘夹", "小碗", "盘子", "碟子", "杯子", "勺子", "罐子", "盒子", "瓶子", "袋子", "锅子",
)
# Single characters that mark a tool when they end the name (e.g. 砂锅, 利口酒杯).
TOOL_SUFFIX = "锅刀铲勺杯碗盘碟盆板筷叉笼箱炉机器秤签纸膜筛网袋瓶盒巾布刷盖具计壶罐杖模灶煲匙瓢锤称夹"
# Foods whose names contain a tool keyword or end with a tool suffix.
TOOL_EXCLUDE = (
    "昆布", "糯米纸", "鱼板", "韭菜盒", "藕盒", "茄盒", "菜盒", "肉盒", "蛋盒",
)

_INGREDIENT_WORDS = ("原料", "食材", "配料", "材料", "调料", "调味", "主料", "辅料", "腌料", "馅料", "酱料", "香料")
_TOOL_WORDS = ("工具", "用具", "器具", "设备", "厨具")

_IMG_RE = re.compile(
    r"!\[[^\]]*\]\((?:[^()]|\([^()]*\))*\)"  # inline ![alt](src)
    r"|!\[[^\]]*\]\[[^\]]*\]"  # reference ![alt][label]
    r"|<img\b[^>]*>",
    re.I,
)
_IMG_REF_RE = re.compile(r"!\[([^\]]*)\]\[([^\]]*)\]")
_REF_DEF_RE = re.compile(r"^\s{0,3}\[([^\]]+)\]:\s*\S+")
_LINK_RE = re.compile(r"\[([^\]]*)\]\((?:[^()]|\([^()]*\))*\)")
_BRACKET_RE = re.compile(r"[（(][^（）()]*[）)]|【[^】]*】|\[[^\]]*\]")
_BOILERPLATE_RE = re.compile(r"如果您遵循本指南的制作流程")
_BULLET_RE = re.compile(r"^(?:[-*+]|\d+[.、)])\s+(.*)$")
_EMPTY_MARKER_RE = re.compile(r"^(?:>\s*)+$|^[-*+]$")
_EXACT_HEADERS = frozenset(_INGREDIENT_WORDS + _TOOL_WORDS)
_FENCE_RE = re.compile(r"^\s*(?:```|~~~)")


def category_zh(category: str) -> str:
    return CATEGORY_ZH.get(category, category)


def recipe_id(rel_path: str) -> str:
    """``vegetable_dish/西红柿炒鸡蛋.md`` -> ``vegetable_dish/西红柿炒鸡蛋``."""
    rel = unicodedata.normalize("NFC", rel_path.replace("\\", "/"))
    return rel[:-3] if rel.endswith(".md") else rel


def source_url(rel_path: str, commit: str = COMMIT) -> str:
    rel = unicodedata.normalize("NFC", rel_path.replace("\\", "/"))
    encoded = "/".join(quote(seg, safe="") for seg in rel.split("/"))
    return f"{REPO_URL}/blob/{commit}/dishes/{encoded}"


def _strip_images(lines: list[str]) -> list[str]:
    used = {
        (label or alt).strip().casefold()
        for line in lines
        for alt, label in _IMG_REF_RE.findall(line)
    }
    out = []
    for line in lines:
        m = _REF_DEF_RE.match(line)
        if m and m.group(1).strip().casefold() in used:
            continue
        cleaned = _IMG_RE.sub("", line)
        if cleaned != line and cleaned.strip() in ("", ">"):
            continue
        out.append(cleaned.rstrip() if cleaned != line else line)
    return out


def _plain(text: str) -> str:
    text = _LINK_RE.sub(r"\1", text)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\*\*|__|\*(?=\S)|(?<=\S)\*", "", text)
    return text.strip()


def _split_sections(lines: list[str]) -> tuple[list[str], list[tuple[str, list[str]]]]:
    """Split on level-2 headings, ignoring ``## `` inside code fences."""
    preamble: list[str] = []
    sections: list[tuple[str, list[str]]] = []
    current = preamble
    in_fence = False
    for line in lines:
        if _FENCE_RE.match(line):
            in_fence = not in_fence
        elif not in_fence and line.startswith("## "):
            current = []
            sections.append((line[3:].strip(), current))
            continue
        current.append(line)
    return preamble, sections


def _split_items(lines: list[str]) -> list[str]:
    """One item per top-level line; indented lines and code fences stay attached."""
    items: list[list[str]] = []
    in_fence = False
    for line in lines:
        is_fence = bool(_FENCE_RE.match(line))
        if in_fence:
            items[-1].append(line)
            if is_fence:
                in_fence = False
            continue
        if not line.strip() or _EMPTY_MARKER_RE.match(line.strip()):
            continue
        indented = line[0] in " \t"
        if indented and items:
            items[-1].append(line.rstrip())
        else:
            items.append([line.rstrip()])
        if is_fence:
            in_fence = True
    return ["\n".join(item).rstrip() for item in items]


def _split_top_level(text: str, seps: str) -> list[str]:
    """Split on any character of ``seps`` outside brackets (not on 1,000)."""
    parts, depth, buf = [], 0, []
    for i, ch in enumerate(text):
        if ch in "（([【":
            depth += 1
        elif ch in "）)]】":
            depth = max(0, depth - 1)
        thousands = ch == "," and text[i - 1 : i].isdigit() and text[i + 1 : i + 2].isdigit()
        if ch in seps and depth == 0 and not thousands:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    parts.append("".join(buf))
    return [p.strip() for p in parts if p.strip()]


def clean_name(raw: str) -> str:
    """Drop links, bracketed notes, the 可选 marker and trailing remarks."""
    text = _LINK_RE.sub(r"\1", raw)
    text = re.sub(r"\*\*|__|`", "", text)
    text = _BRACKET_RE.sub("", text)
    text = re.sub(r"[（(【\[].*$", "", text)  # unclosed opening bracket drops the rest
    text = re.sub(r"[）)】\]]", "", text)
    text = text.replace("*", "")
    text = re.sub(r"^\s*可选\s*[：:]?", "", text)
    text = text.replace("可选", "")
    text = re.split(r"——|—| - ", text, maxsplit=1)[0]
    return text.strip(" \t，,；;：:。.")


_QTY_TAIL_RE = re.compile(r"\s*(?:[\d.]+|[一二三四五六七八九十两])\s*[个只盒袋包瓶双把张块片根套副口]?\s*$")


def _is_tool_name(name: str) -> bool:
    """True when any ``或`` / ``/`` alternative is a tool and none is a known food."""
    name = _QTY_TAIL_RE.sub("", name)
    for piece in re.split(r"[+、，,或/]", name):
        piece = piece.strip()
        if not piece:
            continue
        if any(food in piece for food in TOOL_EXCLUDE):
            continue
        if any(k in piece for k in TOOL_CONTAINS) or piece[-1] in TOOL_SUFFIX:
            return True
    return False


def _group_header(text: str) -> tuple[str | None, bool] | None:
    """Recognise a short group label such as ``### 工具`` or ``可选工具``.

    Returns ``(kind, optional)`` or None when the line is prose.
    """
    plain = _plain(text.lstrip("#").strip()).strip("：:").strip()
    if not plain:
        return None
    is_marked = text.lstrip().startswith("#") or bool(re.match(r"^\*\*.+\*\*\s*[：:]?$", text.strip()))
    if not is_marked and (len(plain) > 12 or re.search(r"[。，；,;]", plain)):
        return None
    kind = None
    if "介绍" in plain or "说明" in plain:
        kind = "skip"
    elif any(w in plain for w in _TOOL_WORDS):
        kind = "tool"
    elif any(w in plain for w in _INGREDIENT_WORDS):
        kind = "ingredient"
    return kind, "可选" in plain


def _bullet_is_header(body: str) -> tuple[str | None, bool] | None:
    """A bullet that is only a group label, e.g. ``- 原料:`` or ``- 工具``."""
    plain = _plain(body).strip("：:").strip()
    core = re.sub(r"^(所需|所必要|必备|必须|必要)的?", "", plain.replace("可选", "")).strip()
    return _group_header(plain) if core in _EXACT_HEADERS else None


MAX_NAME_LEN = 22
_FRAGMENT_RE = re.compile(r"^(?:需|其中|也可以|能够|例如|可以|建议|比如)")


def _split_body(body: str) -> list[str]:
    """Split one bullet into candidate items on ``、``, ``，``, ``；`` and ``+``.

    Comma and plus splits are skipped when they would cut a sentence into
    fragments or separate a quantity (``清水 720g + 600g``).
    """
    out: list[str] = []
    for piece in _split_top_level(body, "、"):
        parts = _split_top_level(piece, "，,；;")
        if len(parts) < 2 or re.search(r"[：:]", piece) or not all(
            len(clean_name(p)) <= 12 and not _FRAGMENT_RE.match(p) for p in parts
        ):
            parts = [piece]
        for part in parts:
            plus = _split_top_level(part, "+")
            if len(plus) > 1 and not any(re.match(r"[\d.]", p) for p in plus):
                out.extend(plus)
            else:
                out.append(part)
    return out


def _parse_ingredients(lines: list[str], quantity_names: list[str]):
    items: list[dict] = []
    notes: list[str] = []
    ctx_kind: str | None = None
    ctx_optional = False
    for line in lines:
        if _FENCE_RE.match(line):
            continue
        text = line.strip()
        if not text or (set(text) <= {"-", "*", "_", " "} and not _BULLET_RE.match(text)):
            continue
        text = re.sub(r"^>\s*", "", text)
        m = _BULLET_RE.match(text)
        if not m:
            header = _group_header(text)
            if header is not None:
                ctx_kind, ctx_optional = header
            elif _plain(text.lstrip(">")):
                notes.append(_plain(text.lstrip(">")))
            continue
        body = m.group(1).strip()
        header = _bullet_is_header(body)
        if header is not None:
            ctx_kind, ctx_optional = header
            continue
        if (
            ctx_kind == "skip"
            or re.match(r"^注[：:意]", body)
            or re.search(r"[！!。？]", _BRACKET_RE.sub("", body).rstrip("。."))
        ):
            notes.append(_plain(body))
            continue
        prefix = ""
        if mp := re.match(r"^可选\s*[：:]\s*", body):
            prefix, body = mp.group(0), body[mp.end() :]
        for piece in _split_body(body):
            raw = prefix + piece
            name = clean_name(raw) or raw
            if ctx_kind == "tool" or _is_tool_name(name):
                kind = "tool"
            elif len(name) > MAX_NAME_LEN and not re.search(r"\d", name):
                notes.append(_plain(raw))
                continue
            elif ctx_kind == "ingredient" or _in_quantities(name, quantity_names):
                kind = "ingredient"
            else:
                kind = "unknown"
            items.append(
                {
                    "raw": raw,
                    "name": name,
                    "optional": ctx_optional or bool(prefix) or "可选" in raw,
                    "kind": kind,
                }
            )
    return items, notes


def _in_quantities(name: str, quantity_names: list[str]) -> bool:
    if not name:
        return False
    for q in quantity_names:
        if q == name or (len(name) >= 2 and len(q) >= 2 and (name in q or q in name)):
            return True
    return False


_QTY_SPLIT_RE = re.compile(r"^(?P<name>[^\s\d（(=＝:：]+)\s*(?P<expr>.*)$")


def _parse_quantities(lines: list[str]):
    rows: list[dict] = []
    notes: list[str] = []
    table_rows: list[dict] = []
    for line in lines:
        text = line.strip()
        if not text or _FENCE_RE.match(line):
            continue
        if text.startswith("|"):
            cells = [c.strip() for c in text.strip("|").split("|")]
            if all(re.fullmatch(r":?-+:?", c) or not c for c in cells):
                # Separator row: the row just before it was a header, not data.
                if table_rows:
                    rows.remove(table_rows.pop())
                table_rows = []
                continue
            if not cells[0]:
                continue
            row = {"name": cells[0], "expr": " ".join(c for c in cells[1:] if c)}
            rows.append(row)
            table_rows.append(row)
            continue
        table_rows = []
        text = re.sub(r"^>\s*", "", text)
        m = _BULLET_RE.match(text)
        if not m:
            if text.startswith("#") or (len(text) <= 12 and text[-1] in "：:"):
                continue
            notes.append(_plain(text))
            continue
        body = m.group(1).strip()
        eq = re.search(r"[=＝]", body)
        if eq:
            name, expr = body[: eq.start()], body[eq.end() :]
        else:
            mm = _QTY_SPLIT_RE.match(_LINK_RE.sub(r"\1", body))
            name, expr = (mm.group("name"), mm.group("expr")) if mm else (body, "")
        name = _LINK_RE.sub(r"\1", name).replace("**", "").strip()
        rows.append({"name": name, "expr": expr.strip()})
    return rows, notes


def _is_rule(text: str) -> bool:
    return bool(re.fullmatch(r"\s*([-*_]\s*){3,}", text))


def _parse_steps(lines: list[str]) -> list[str]:
    return [item for item in _split_items(lines) if not _is_rule(item)]


def _clean_notes(sections: list[tuple[str, list[str]]]) -> str:
    chunks = []
    for title, lines in sections:
        lines = [ln for ln in lines if not _BOILERPLATE_RE.search(ln)]
        body = "\n".join(_trim_rules(_collapse_blank(lines))).strip()
        if not body:
            continue
        chunks.append(body if title == "附加内容" else f"## {title}\n\n{body}")
    return "\n\n".join(chunks)


def _trim_rules(lines: list[str]) -> list[str]:
    """Drop the horizontal rules HowToCook puts between sections."""
    while lines and (not lines[-1].strip() or _is_rule(lines[-1])):
        lines = lines[:-1]
    while lines and (not lines[0].strip() or _is_rule(lines[0])):
        lines = lines[1:]
    return lines


def _collapse_blank(lines: list[str]) -> list[str]:
    out: list[str] = []
    for ln in lines:
        if not ln.strip() and (not out or not out[-1].strip()):
            continue
        out.append(ln.rstrip())
    return out


def parse_recipe(text: str, rel_path: str, commit: str = COMMIT):
    """Return ``(record, issues)``; ``record`` is None when nothing usable was found.

    ``rel_path`` is relative to ``dishes/`` (e.g. ``meat_dish/红烧肉/南派红烧肉.md``).
    """
    text = text.replace("\r\n", "\n").replace("\r", "\n").lstrip("﻿")
    lines = _strip_images(text.split("\n"))
    preamble, sections = _split_sections(lines)

    issues: list[str] = []
    rel = unicodedata.normalize("NFC", rel_path.replace("\\", "/"))
    category = rel.split("/")[0]
    stem = rel.rsplit("/", 1)[-1][:-3] if rel.endswith(".md") else rel.rsplit("/", 1)[-1]

    title = None
    summary_lines: list[str] = []
    difficulty = None
    calories = None
    for line in preamble:
        if title is None and line.startswith("# "):
            title = line[2:].strip()
            continue
        if m := re.search(r"预估烹饪难度\s*[：:]\s*(.*)", line):
            stars = m.group(1).count("★")
            difficulty = stars or None
            continue
        if m := re.search(r"预估卡路里\s*[：:]\s*(\d+)", line):
            calories = int(m.group(1))
            continue
        if title is not None:
            summary_lines.append(line)
    if title is None:
        issues.append("missing:title")
    name = re.sub(r"的做法$", "", title).strip() if title else stem
    if difficulty is None:
        issues.append("missing:difficulty")
    if calories is None:
        issues.append("missing:calories")
    summary = "\n\n".join(
        _plain(" ".join(p)) for p in _paragraphs(summary_lines)
    ).strip()

    ingredient_lines: list[str] | None = None
    quantity_lines: list[str] | None = None
    step_lines: list[str] | None = None
    other: list[tuple[str, list[str]]] = []
    for title_, body in sections:
        if "必备原料" in title_ or title_.startswith("原料"):
            ingredient_lines = (ingredient_lines or []) + body
        elif title_.startswith("计算"):
            quantity_lines = (quantity_lines or []) + body
        elif title_.startswith("操作"):
            step_lines = (step_lines or []) + body
        else:
            other.append((title_, body))

    quantities, quantity_notes = _parse_quantities(quantity_lines or [])
    qnames = [clean_name(q["name"]) for q in quantities if q["name"]]
    ingredients, ingredient_notes = _parse_ingredients(ingredient_lines or [], qnames)
    steps = _parse_steps(step_lines or [])

    if not ingredients:
        issues.append("missing:ingredients")
    if not steps:
        issues.append("missing:steps")
    if not quantities:
        issues.append("missing:quantities")

    if not title and not ingredients and not steps:
        return None, issues

    record = {
        "id": recipe_id(rel),
        "name": name,
        "category": category,
        "category_zh": category_zh(category),
        "difficulty": difficulty,
        "calories_kcal": calories,
        "time_minutes": None,
        "summary": summary,
        "ingredients": ingredients,
        "ingredient_notes": ingredient_notes,
        "quantities": quantities,
        "quantity_notes": quantity_notes,
        "steps": steps,
        "notes": _clean_notes(other),
        "source_url": source_url(rel, commit),
    }
    return record, issues


def _paragraphs(lines: list[str]) -> list[list[str]]:
    paras: list[list[str]] = []
    cur: list[str] = []
    for ln in lines:
        stripped = re.sub(r"^>\s*", "", ln.strip())
        if stripped:
            cur.append(stripped)
        elif cur:
            paras.append(cur)
            cur = []
    if cur:
        paras.append(cur)
    return paras
