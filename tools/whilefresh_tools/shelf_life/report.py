"""Markdown coverage report: per-category coverage, uncovered ingredients, spot-check sample."""

import random

from ..vocabulary.schema import CATEGORIES
from .foodkeeper import product_label
from .schema import shelf_life_warnings


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


def make_report(vocab_doc, shelf_doc, map_doc, products, sample: int = 40, seed: int = 20261021) -> str:
    by_name = {it["name"]: it for it in vocab_doc["items"]}
    covered = set(shelf_doc["items"])
    out: list[str] = []

    out += ["### 覆盖率", "", "| 品类 | 标准名数 | 直接有参考值 | 直接占比 | 按 parent 回退后 | 回退后占比 |", "| --- | --- | --- | --- | --- | --- |"]
    for code, label, total, direct, fallback in coverage_rows(vocab_doc, shelf_doc):
        name = f"{label}（{code}）" if code else label
        out.append(f"| {name} | {total} | {direct} | {_pct(direct, total)} | {fallback} | {_pct(fallback, total)} |")

    only_fallback = sorted(n for n in by_name if n not in covered and covering_ancestor(n, by_name, covered))
    out += ["", f"仅靠 parent 回退才有参考值的标准名 {len(only_fallback)} 个：" + "、".join(f"{n}→{covering_ancestor(n, by_name, covered)}" for n in only_fallback), ""]

    order = {code: i for i, (code, _) in enumerate(CATEGORIES)}
    uncovered = sorted(
        (n for n, it in by_name.items() if not covering_ancestor(n, by_name, covered) and not it["pantry_default"]),
        key=lambda n: (order[by_name[n]["category"]], n),
    )
    skipped_pantry = sum(1 for n, it in by_name.items() if it["pantry_default"] and not covering_ancestor(n, by_name, covered))
    out += [
        "### 无参考值的食材",
        "",
        f"口径：回退后仍无参考值、且不是默认常备的标准名，共 {len(uncovered)} 个（另有 {skipped_pantry} 个默认常备项无参考值，不列入）。",
        "",
        "| 标准名 | 品类 |",
        "| --- | --- |",
    ]
    out += [f"| {n} | {by_name[n]['category']} |" for n in uncovered]

    names = sorted(shelf_doc["items"])
    picked = sorted(random.Random(seed).sample(names, min(sample, len(names))))
    out += ["", f"### 抽样对照（{len(picked)} 条，种子 {seed}）", "", "| 标准名 | FoodKeeper 条目 | 冷藏 | 冷冻 | 常温 | 取值字段 |", "| --- | --- | --- | --- | --- | --- |"]
    for n in picked:
        it = shelf_doc["items"][n]
        label = f"{it['foodkeeper_id']} {product_label(products[it['foodkeeper_id']])}"
        basis = ", ".join(f"{m}:{c}" for m, c in it["basis"].items())
        cells = [str(it[k]) if it[k] is not None else "-" for k in ("fridge_days", "freezer_days", "room_days")]
        out.append(f"| {n} | {label} | {cells[0]} | {cells[1]} | {cells[2]} | {basis} |")

    overrides = [(n, e) for n, e in sorted(map_doc["map"].items()) if e.get("override")]
    out += ["", f"### 人工修正值（{len(overrides)} 个标准名）", "", "| 标准名 | 修正的存放方式与天数 | 理由 |", "| --- | --- | --- |"]
    for n, e in overrides:
        values = "、".join(f"{m} {d}" for m, d in e["override"].items())
        out.append(f"| {n} | {values} | {e['reason']} |")

    ripe = sorted(n for n, it in shelf_doc["items"].items() if any(c.endswith("(ripe)") for c in it["basis"].values()))
    out += ["", f"### 仅适用于成熟果的冷藏值（{len(ripe)} 个）", "", "、".join(ripe) if ripe else "无。"]

    warnings = shelf_life_warnings(shelf_doc, map_doc)
    out += ["", "### 校验警告", ""]
    out += [f"- {w}" for w in warnings] if warnings else ["无。"]
    return "\n".join(out) + "\n"
