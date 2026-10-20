"""CLI: download/reuse the pinned HowToCook snapshot, parse it, write recipes.json."""

import argparse
import json
import sys
import unicodedata
from collections import Counter
from pathlib import Path

from .fetch import COMMIT, REPO_URL, FetchError, default_cache_dir, ensure_snapshot
from .parse import parse_recipe
from ..vocabulary.mapping import MissingMapping, annotate_recipes, load_map
from ..vocabulary.schema import MAP_PATH

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUT = REPO_ROOT / "app" / "assets" / "data" / "recipes.json"
DEFAULT_REPORT = REPO_ROOT / "tools" / "reports" / "recipes-report.md"

SOURCE = {
    "name": "HowToCook",
    "url": REPO_URL,
    "commit": COMMIT,
    "license": "Unlicense",
    "license_url": f"{REPO_URL}/blob/{COMMIT}/LICENSE",
}


def collect_files(dishes_dir: Path) -> list[Path]:
    """All recipe files under ``dishes/`` except the ``template/`` example."""
    files = [
        p
        for p in dishes_dir.rglob("*.md")
        if p.relative_to(dishes_dir).parts[0] != "template"
    ]
    return sorted(files, key=lambda p: unicodedata.normalize("NFC", p.relative_to(dishes_dir).as_posix()))


def build_document(dishes_dir: Path, commit: str = COMMIT, ingredient_map: dict | None = None):
    """Return ``(document, report_data)``.

    With ``ingredient_map`` every ingredient also gets ``canonical`` (standard
    names; ``[]`` for tools and ignored items); a name missing from the map raises
    ``MissingMapping``.
    """
    recipes = []
    issues: dict[str, list[str]] = {}
    skipped: list[str] = []
    for path in collect_files(dishes_dir):
        rel = path.relative_to(dishes_dir).as_posix()
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            skipped.append(f"{rel} (unreadable: {exc})")
            continue
        record, found = parse_recipe(text, rel, commit)
        if record is None:
            skipped.append(f"{rel} ({', '.join(found) or 'unparseable'})")
            continue
        if found:
            issues[record["id"]] = found
        recipes.append(record)
    recipes.sort(key=lambda r: r["id"])
    doc = {"source": {**SOURCE, "commit": commit}, "recipes": recipes}
    if ingredient_map is not None:
        annotate_recipes(doc, ingredient_map)
    return doc, {"issues": issues, "skipped": skipped}


def dump(doc: dict) -> str:
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"


def format_report(doc: dict, data: dict) -> str:
    recipes = doc["recipes"]
    out = [f"# HowToCook 菜谱解析报告 ({doc['source']['commit'][:10]})", ""]
    out.append(f"收录总数: {len(recipes)}")
    out.append(f"跳过(无法解析): {len(data['skipped'])}")
    out.append("")
    out.append("## 各分类数量")
    for cat, n in sorted(Counter(r["category"] for r in recipes).items()):
        out.append(f"- {cat}: {n}")
    kinds = Counter(i["kind"] for r in recipes for i in r["ingredients"])
    out += ["", "## 原料项 kind 统计"]
    out += [f"- {k}: {kinds.get(k, 0)}" for k in ("ingredient", "tool", "unknown")]
    out += ["", "## 跳过的文件"]
    out += [f"- {s}" for s in data["skipped"]] or ["- (无)"]
    out += ["", "## 缺节或缺字段的菜谱"]
    for rid, found in sorted(data["issues"].items()):
        out.append(f"- {rid}: {', '.join(found)}")
    if not data["issues"]:
        out.append("- (无)")
    unknown = Counter(
        i["name"] for r in recipes for i in r["ingredients"] if i["kind"] == "unknown"
    )
    out += ["", "## kind=unknown 原料项频次前 50"]
    out += [f"- {name}: {n}" for name, n in unknown.most_common(50)]
    return "\n".join(out) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="python -m whilefresh_tools.recipes")
    sub = parser.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="download (or reuse cache), parse and write recipes.json")
    b.add_argument("--cache-dir", type=Path, default=None, help="snapshot cache directory")
    b.add_argument("--out", type=Path, default=DEFAULT_OUT)
    b.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    b.add_argument("--ingredient-map", type=Path, default=MAP_PATH, help="recipe ingredient -> standard names")
    args = parser.parse_args(argv)

    try:
        cache = ensure_snapshot(args.cache_dir or default_cache_dir())
    except FetchError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    try:
        doc, data = build_document(cache / "dishes", ingredient_map=load_map(args.ingredient_map))
    except (MissingMapping, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(dump(doc))
    report = format_report(doc, data)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with open(args.report, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(report)
    sys.stdout.write(report)
    print(f"\nwrote {args.out} ({args.out.stat().st_size} bytes)")
    return 0
