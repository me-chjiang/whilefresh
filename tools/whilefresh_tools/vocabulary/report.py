"""Statistics and a reproducible mapping sample for the review report."""

import random
from collections import Counter

from .schema import CATEGORIES, recipe_ingredient_names


def make_report(vocab: dict, map_doc: dict, recipes: dict, sample: int, seed: int) -> str:
    items = vocab["items"]
    mapping = map_doc["map"]
    counts = Counter(i["category"] for i in items)
    needed = recipe_ingredient_names(recipes)
    mapped = [n for n in needed if mapping.get(n)]
    empty = sorted(n for n in needed if not mapping.get(n))
    occurrences = [
        ing for r in recipes["recipes"] for ing in r["ingredients"] if ing["kind"] != "tool"
    ]
    occ_mapped = sum(1 for ing in occurrences if mapping.get(ing["name"]))
    used = {t for ts in mapping.values() for t in ts}

    out = ["## 规模统计", "", f"- 标准名总数：{len(items)}"]
    out += [f"  - {zh}（{code}）：{counts.get(code, 0)}" for code, zh in CATEGORIES]
    out += [
        f"- 别名总数：{sum(len(i['aliases']) for i in items)}",
        f"- 有 parent 的项：{sum(1 for i in items if i.get('parent'))}",
        f"- pantry_default：{sum(i['pantry_default'] for i in items)}",
        f"- bulk_default：{sum(i['bulk_default'] for i in items)}",
        f"- 被菜谱映射用到的标准名：{len(used)}（其余 {len(items) - len(used)} 项来自超市商品补充）",
        f"- 菜谱非工具原料写法：{len(needed)} 种，映射到至少一个标准名：{len(mapped)}（{len(mapped) / len(needed):.1%}）",
        f"- 菜谱非工具原料行：{len(occurrences)} 行，有标准名：{occ_mapped}（{occ_mapped / len(occurrences):.1%}）",
        f"- 映射为空（无需匹配）的写法：{len(empty)} 种",
        "",
        "## 随机抽样映射（seed %d，共 %d 条）" % (seed, sample),
        "",
        "| 原料写法 | 标准名 |",
        "| --- | --- |",
    ]
    rng = random.Random(seed)
    for name in rng.sample(sorted(needed), min(sample, len(needed))):
        targets = "、".join(mapping[name]) if mapping[name] else "（忽略）"
        out.append(f"| {name} | {targets} |")
    out += ["", "## 映射为空的写法", "", "、".join(empty) or "(无)", ""]
    return "\n".join(out)
