"""CLI: draft (AI, one-off), build vocabulary.json, validate, report."""

import argparse
import sys
from pathlib import Path

from .schema import (
    MAP_PATH,
    RECIPES_PATH,
    SOURCE_PATH,
    VOCABULARY_OUT,
    categories_doc,
    dump_map,
    dump_vocabulary,
    load_json,
    sorted_items,
    validate_all,
    validate_map,
    validate_vocabulary,
)


def build_vocabulary(source: dict) -> dict:
    """Deterministic: normalise key order, sort by category then name."""
    return {
        "version": source["version"],
        "categories": categories_doc(),
        "items": sorted_items(source["items"]),
    }


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def cmd_build(args) -> int:
    source = load_json(args.source)
    doc = build_vocabulary(source)
    errors = validate_vocabulary(doc)
    errors += validate_map(load_json(args.map), {i["name"] for i in doc["items"]})
    if errors:
        _print_errors(errors)
        return 1
    write_text(args.out, dump_vocabulary(doc))
    print(f"wrote {args.out} ({len(doc['items'])} items)")
    return 0


def cmd_validate(args) -> int:
    vocab = load_json(args.vocabulary)
    errors = validate_all(vocab, load_json(args.map), load_json(args.recipes))
    if vocab != build_vocabulary(load_json(args.source)):
        errors.append("vocabulary.json is stale: rebuild it from vocabulary-source.json")
    if errors:
        _print_errors(errors)
        return 1
    print("ok")
    return 0


def _print_errors(errors: list[str]) -> None:
    for e in errors[:200]:
        print("error:", e, file=sys.stderr)
    if len(errors) > 200:
        print(f"... and {len(errors) - 200} more", file=sys.stderr)
    print(f"{len(errors)} problem(s)", file=sys.stderr)


def cmd_draft(args) -> int:
    from .deepseek import Client, DeepSeekError
    from .draft import run_draft

    client = Client(cache_dir=args.cache_dir)
    try:
        items, mapping, log = run_draft(client, load_json(args.recipes))
    except DeepSeekError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    out = args.out_dir
    doc = {"version": 1, "categories": categories_doc(), "items": sorted_items(items)}
    write_text(out / "vocabulary-source.json", dump_vocabulary(doc))
    write_text(out / "ingredient-map.json", dump_map({"version": 1, "map": mapping}))
    write_text(out / "draft-log.txt", "\n".join(log) + "\n")
    print(
        f"draft: {len(items)} items, {len(mapping)} mappings, "
        f"{client.requests} request(s), {client.cache_hits} cache hit(s), usage {client.usage}"
    )
    print(f"wrote draft files to {out}; copy them over the source files after review")
    return 0


def cmd_report(args) -> int:
    from .report import make_report

    sys.stdout.write(
        make_report(
            load_json(args.vocabulary), load_json(args.map), load_json(args.recipes), args.sample, args.seed
        )
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    from .deepseek import default_cache_dir

    parser = argparse.ArgumentParser(prog="python -m whilefresh_tools.vocabulary")
    sub = parser.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("draft", help="one-off: ask DeepSeek for a draft vocabulary and ingredient map")
    d.add_argument("--recipes", type=Path, default=RECIPES_PATH)
    d.add_argument("--cache-dir", type=Path, default=None, help="response cache (default ~/.cache/whilefresh/vocabulary)")
    d.add_argument("--out-dir", type=Path, default=default_cache_dir() / "draft")
    d.set_defaults(func=cmd_draft)

    b = sub.add_parser("build", help="vocabulary-source.json -> app/assets/data/vocabulary.json")
    b.add_argument("--source", type=Path, default=SOURCE_PATH)
    b.add_argument("--map", type=Path, default=MAP_PATH)
    b.add_argument("--out", type=Path, default=VOCABULARY_OUT)
    b.set_defaults(func=cmd_build)

    v = sub.add_parser("validate", help="check vocabulary, ingredient map and recipes.json")
    v.add_argument("--source", type=Path, default=SOURCE_PATH)
    v.add_argument("--vocabulary", type=Path, default=VOCABULARY_OUT)
    v.add_argument("--map", type=Path, default=MAP_PATH)
    v.add_argument("--recipes", type=Path, default=RECIPES_PATH)
    v.set_defaults(func=cmd_validate)

    r = sub.add_parser("report", help="print statistics and a seeded mapping sample (markdown)")
    r.add_argument("--vocabulary", type=Path, default=VOCABULARY_OUT)
    r.add_argument("--map", type=Path, default=MAP_PATH)
    r.add_argument("--recipes", type=Path, default=RECIPES_PATH)
    r.add_argument("--sample", type=int, default=60)
    r.add_argument("--seed", type=int, default=2026)
    r.set_defaults(func=cmd_report)

    args = parser.parse_args(argv)
    return args.func(args)
