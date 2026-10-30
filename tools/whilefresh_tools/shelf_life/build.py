"""CLI: draft (AI, one-off), build shelf-life.json, validate, report."""

import argparse
import sys
from pathlib import Path

from ..vocabulary.schema import VOCABULARY_OUT, load_json
from .foodkeeper import load_products
from .schema import (
    MAP_PATH,
    SHELF_LIFE_OUT,
    SNAPSHOT_PATH,
    build_shelf_life,
    dump_map,
    dump_shelf_life,
    map_entries_without_values,
    shelf_life_warnings,
    validate_map,
    validate_shelf_life,
)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def _print_errors(errors: list[str]) -> None:
    for e in errors[:200]:
        print("error:", e, file=sys.stderr)
    if len(errors) > 200:
        print(f"... and {len(errors) - 200} more", file=sys.stderr)
    print(f"{len(errors)} problem(s)", file=sys.stderr)


def _check_inputs(map_doc: dict, products: dict, vocab_items: list[dict]):
    """Map errors and the built document, or the errors alone."""
    errors = validate_map(map_doc, {i["name"] for i in vocab_items}, products)
    if errors:
        return errors, None
    built = build_shelf_life(map_doc, products, vocab_items)
    errors = [f"{n!r} is in the map but FoodKeeper gives it no usable number (remove it or change the id)" for n in map_entries_without_values(map_doc, built)]
    errors += validate_shelf_life(built, {i["name"] for i in vocab_items}, products)
    return errors, built


def cmd_build(args) -> int:
    products = load_products(args.snapshot)
    vocab_items = load_json(args.vocabulary)["items"]
    errors, built = _check_inputs(load_json(args.map), products, vocab_items)
    if errors:
        _print_errors(errors)
        return 1
    write_text(args.out, dump_shelf_life(built))
    print(f"wrote {args.out} ({len(built['items'])} items)")
    for w in shelf_life_warnings(built, load_json(args.map)):
        print("warning:", w, file=sys.stderr)
    return 0


def cmd_validate(args) -> int:
    products = load_products(args.snapshot)
    vocab_items = load_json(args.vocabulary)["items"]
    map_doc = load_json(args.map)
    errors, built = _check_inputs(map_doc, products, vocab_items)
    if built is not None:
        try:
            current = args.out.read_text(encoding="utf-8")
        except OSError:
            current = None
        if current != dump_shelf_life(built):
            errors.append(f"{args.out.name} is stale: rebuild it from foodkeeper-map.json")
        warnings = shelf_life_warnings(built, map_doc)
        for w in warnings:
            print("warning:", w, file=sys.stderr)
    if errors:
        _print_errors(errors)
        return 1
    print("ok")
    return 0


def cmd_report(args) -> int:
    from .report import make_report

    products = load_products(args.snapshot)
    sys.stdout.write(
        make_report(
            load_json(args.vocabulary),
            load_json(args.out),
            load_json(args.map),
            products,
            args.sample,
            args.seed,
        )
    )
    return 0


def cmd_draft(args) -> int:
    from ..vocabulary.deepseek import Client, DeepSeekError
    from .draft import run_draft

    client = Client(cache_dir=args.cache_dir)
    try:
        entries, log = run_draft(client, load_products(args.snapshot), load_json(args.vocabulary)["items"])
    except (DeepSeekError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    out = args.out_dir
    write_text(out / "foodkeeper-map.json", dump_map({"version": 1, "foodkeeper_version": "v128", "map": entries}))
    write_text(out / "draft-log.txt", "\n".join(log) + "\n")
    print(
        f"draft: {len(entries)} mapped, {client.requests} request(s), "
        f"{client.cache_hits} cache hit(s), usage {client.usage}"
    )
    print(f"wrote draft files to {out}; copy them over the source file after review")
    return 0


def default_cache_dir() -> Path:
    from ..vocabulary.deepseek import default_cache_dir as vocab_cache

    return vocab_cache().parent / "shelf-life"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="python -m whilefresh_tools.shelf_life")
    sub = parser.add_subparsers(dest="cmd", required=True)

    def common(p, with_out=True):
        p.add_argument("--snapshot", type=Path, default=SNAPSHOT_PATH)
        p.add_argument("--vocabulary", type=Path, default=VOCABULARY_OUT)
        p.add_argument("--map", type=Path, default=MAP_PATH)
        if with_out:
            p.add_argument("--out", type=Path, default=SHELF_LIFE_OUT)

    d = sub.add_parser("draft", help="one-off: ask DeepSeek for a draft standard name -> FoodKeeper map")
    common(d, with_out=False)
    d.add_argument("--cache-dir", type=Path, default=default_cache_dir(), help="response cache (default ~/.cache/whilefresh/shelf-life)")
    d.add_argument("--out-dir", type=Path, default=default_cache_dir() / "draft")
    d.set_defaults(func=cmd_draft)

    b = sub.add_parser("build", help="foodkeeper-map.json + snapshot -> app/assets/data/shelf-life.json")
    common(b)
    b.set_defaults(func=cmd_build)

    v = sub.add_parser("validate", help="check the map, the generated file and that it is up to date")
    common(v)
    v.set_defaults(func=cmd_validate)

    r = sub.add_parser("report", help="print the Markdown coverage report")
    common(r)
    r.add_argument("--sample", type=int, default=40)
    r.add_argument("--seed", type=int, default=20261021)
    r.set_defaults(func=cmd_report)

    args = parser.parse_args(argv)
    return args.func(args)
