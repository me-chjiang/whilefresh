"""Markdown coverage report: per-category coverage, uncovered frequent ingredients, spot-check sample."""

import random
from collections import Counter

from ..vocabulary.schema import CATEGORIES
from .foodkeeper import product_label
from .schema import shelf_life_warnings

TOP_UNCOVERED = 50


def recipe_counts(recipes_doc: dict) -> Counter:
    """Standard name -> number of recipes that use it (once per recipe)."""
    counts: Counter = Counter()
    for recipe in recipes_doc["recipes"]:
        names = {n for ing in recipe["ingredients"] for group in ing.get("canonical", []) for n in group}
        counts.update(names)
    return counts


def covering_ancestor(name: str, by_name: dict[str, dict], covered) -> str | None:
    """The name itself, else the nearest ancestor with a reference value."""
    seen = set()
    cur = name
    while cur and cur not in seen:
        if cur in covered:
            return cur
        seen.add(cur)
        cur = (by_name.get(cur) or {}).get("parent")
    return None


def _pct(part: int, whole: int) -> str:
    return f"{100 * part / whole:.1f}%" if whole else "-"


def coverage_rows(vocab_doc: dict, shelf_doc: dict):
    by_name = {it["name"]: it for it in vocab_doc["items"]}
    covered = set(shelf_doc["items"])
    rows = []
    totals = [0, 0, 0]
    for code, label in CATEGORIES:
        names = [n for n, it in by_name.items() if it["category"] == code]
        direct = sum(1 for n in names if n in covered)
        fallback = sum(1 for n in names if covering_ancestor(n, by_name, covered))
        rows.append((code, label, len(names), direct, fallback))
        totals = [totals[0] + len(names), totals[1] + direct, totals[2] + fallback]
    rows.append(("", "合计", *totals))
    return rows


def make_report(vocab_doc, recipes_doc, shelf_doc, map_doc, products, sample: int = 40, seed: int = 20261021) -> str:
    by_name = {it["name"]: it for it in vocab_doc["items"]}
    covered = set(shelf_doc["items"])
    counts = recipe_counts(recipes_doc)
    out: list[str] = []

    out += ["### 覆盖率", "", "| 品类 | 标准名数 | 直接有参考值 | 直接占比 | 按 parent 回退后 | 回退后占比 |", "| --- | --- | --- | --- | --- | --- |"]
    for code, label, total, direct, fallback in coverage_rows(vocab_doc, shelf_doc):
        name = f"{label}（{code}）" if code else label
        out.append(f"| {name} | {total} | {direct} | {_pct(direct, total)} | {fallback} | {_pct(fallback, total)} |")

    weighted = [(counts[n], bool(covering_ancestor(n, by_name, covered))) for n, it in by_name.items() if not it["pantry_default"]]
    total_uses = sum(c for c, _ in weighted)
    covered_uses = sum(c for c, ok in weighted if ok)
    out += [
        "",
        f"按菜谱用料出现次数加权（不含默认常备项，回退后）：{covered_uses} / {total_uses}，{_pct(covered_uses, total_uses)}。",
    ]

    only_fallback = sorted(n for n in by_name if n not in covered and covering_ancestor(n, by_name, covered))
    out += ["", f"仅靠 parent 回退才有参考值的标准名 {len(only_fallback)} 个：" + "、".join(f"{n}→{covering_ancestor(n, by_name, covered)}" for n in only_fallback), ""]

    uncovered = [
        (n, it)
        for n, it in by_name.items()
        if not covering_ancestor(n, by_name, covered) and not it["pantry_default"]
    ]
    uncovered.sort(key=lambda p: (-counts[p[0]], p[0]))
    skipped_pantry = sum(1 for n, it in by_name.items() if it["pantry_default"] and not covering_ancestor(n, by_name, covered))
    out += [
        f"### 无参考值的高频食材（前 {TOP_UNCOVERED}，按菜谱出现次数）",
        "",
        f"口径：回退后仍无参考值、且不是默认常备的标准名，共 {len(uncovered)} 个（另有 {skipped_pantry} 个默认常备项无参考值，不列入）。次数为用到该标准名的菜谱数。",
        "",
        "| # | 标准名 | 品类 | 菜谱数 |",
        "| --- | --- | --- | --- |",
    ]
    for rank, (n, it) in enumerate(uncovered[:TOP_UNCOVERED], 1):
        out.append(f"| {rank} | {n} | {it['category']} | {counts[n]} |")

    names = sorted(shelf_doc["items"])
    picked = sorted(random.Random(seed).sample(names, min(sample, len(names))))
    out += ["", f"### 抽样对照（{len(picked)} 条，种子 {seed}）", "", "| 标准名 | FoodKeeper 条目 | 冷藏 | 冷冻 | 常温 | 取值字段 |", "| --- | --- | --- | --- | --- | --- |"]
    for n in picked:
        it = shelf_doc["items"][n]
        label = f"{it['foodkeeper_id']} {product_label(products[it['foodkeeper_id']])}"
        basis = ", ".join(f"{m}:{c}" for m, c in it["basis"].items())
        cells = [str(it[k]) if it[k] is not None else "-" for k in ("fridge_days", "freezer_days", "room_days")]
        out.append(f"| {n} | {label} | {cells[0]} | {cells[1]} | {cells[2]} | {basis} |")

    warnings = shelf_life_warnings(shelf_doc, map_doc)
    out += ["", "### 校验警告", ""]
    out += [f"- {w}" for w in warnings] if warnings else ["无。"]
    return "\n".join(out) + "\n"
