"""Vocabulary file formats, loading, serialisation and validation."""

import json
from pathlib import Path

PACKAGE_DIR = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[3]
SOURCE_PATH = PACKAGE_DIR / "vocabulary-source.json"
VOCABULARY_OUT = REPO_ROOT / "app" / "assets" / "data" / "vocabulary.json"

# Order is the display order; the codes are a contract with the UI colour mapping.
CATEGORIES = (
    ("vegetable", "蔬菜"),
    ("fruit", "水果"),
    ("meat", "肉类"),
    ("seafood", "水产海鲜"),
    ("egg_dairy", "蛋奶"),
    ("soy", "豆制品"),
    ("staple", "主食粮油"),
    ("seasoning", "调料"),
    ("processed", "加工/速食"),
    ("other", "其他"),
)
CATEGORY_CODES = tuple(code for code, _ in CATEGORIES)

ITEM_KEYS = ("name", "category", "aliases", "parent", "pantry_default", "bulk_default")


def load_json(path: Path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def normalize_item(item: dict) -> dict:
    """Canonical key order; ``parent`` is omitted when absent."""
    out = {
        "name": item["name"],
        "category": item["category"],
        "aliases": list(item.get("aliases", [])),
    }
    if item.get("parent"):
        out["parent"] = item["parent"]
    out["pantry_default"] = bool(item.get("pantry_default", False))
    out["bulk_default"] = bool(item.get("bulk_default", False))
    return out


def _line(obj) -> str:
    return json.dumps(obj, ensure_ascii=False, separators=(", ", ": "))


def dump_vocabulary(doc: dict) -> str:
    """One item per line: reviewable in diffs and greppable."""
    body = ",\n".join("  " + _line(it) for it in doc["items"])
    return (
        '{\n "version": %d,\n "categories": %s,\n "items": [\n%s\n ]\n}\n'
        % (doc["version"], _line(doc["categories"]), body)
    )


def sorted_items(items: list[dict]) -> list[dict]:
    order = {c: i for i, c in enumerate(CATEGORY_CODES)}
    return sorted(
        (normalize_item(i) for i in items),
        key=lambda i: (order.get(i["category"], 99), i["name"]),
    )


def categories_doc() -> list[dict]:
    return [{"code": c, "name": n} for c, n in CATEGORIES]


def norm_key(text: str) -> str:
    """Case- and whitespace-insensitive identity for names and aliases."""
    return "".join(text.split()).casefold()


def validate_vocabulary(doc: dict) -> list[str]:
    errors: list[str] = []
    if not isinstance(doc.get("version"), int):
        errors.append("version must be an integer")
    if doc.get("categories") != categories_doc():
        errors.append("categories must equal the fixed category list")
    items = doc.get("items")
    if not isinstance(items, list) or not items:
        return errors + ["items must be a non-empty list"]

    names: dict[str, dict] = {}
    name_keys: dict[str, str] = {}
    for idx, it in enumerate(items):
        where = f"items[{idx}]"
        if not isinstance(it, dict):
            errors.append(f"{where}: not an object")
            continue
        extra = set(it) - set(ITEM_KEYS)
        if extra:
            errors.append(f"{where} {it.get('name')!r}: unknown keys {sorted(extra)}")
        name = it.get("name")
        if not isinstance(name, str) or not name.strip() or name != name.strip():
            errors.append(f"{where}: bad name {name!r}")
            continue
        if name in names:
            errors.append(f"duplicate standard name {name!r}")
        elif norm_key(name) in name_keys:
            errors.append(f"standard names {name_keys[norm_key(name)]!r} and {name!r} differ only in case or spacing")
        names[name] = it
        name_keys.setdefault(norm_key(name), name)
        if it.get("category") not in CATEGORY_CODES:
            errors.append(f"{name!r}: invalid category {it.get('category')!r}")
        for flag in ("pantry_default", "bulk_default"):
            if not isinstance(it.get(flag), bool):
                errors.append(f"{name!r}: {flag} must be a boolean")
        if not isinstance(it.get("aliases"), list):
            errors.append(f"{name!r}: aliases must be a list")

    alias_owner: dict[str, str] = {}
    for name, it in names.items():
        aliases = it.get("aliases") if isinstance(it.get("aliases"), list) else []
        for alias in aliases:
            if not isinstance(alias, str) or not alias.strip() or alias != alias.strip():
                errors.append(f"{name!r}: bad alias {alias!r}")
                continue
            key = norm_key(alias)
            if key in name_keys:
                errors.append(f"alias {alias!r} of {name!r} collides with a standard name")
            if key in alias_owner:
                errors.append(
                    f"alias {alias!r} used by both {alias_owner[key]!r} and {name!r}"
                )
            alias_owner[key] = name

    for name, it in names.items():
        parent = it.get("parent")
        if parent is None:
            continue
        if not isinstance(parent, str):
            errors.append(f"{name!r}: parent must be a string, got {parent!r}")
        elif parent not in names:
            errors.append(f"{name!r}: parent {parent!r} is not in the vocabulary")
        elif parent == name:
            errors.append(f"{name!r}: parent is itself")
    for name in names:
        seen = [name]
        cur = names[name].get("parent")
        while isinstance(cur, str) and cur in names:
            if cur in seen:
                errors.append("parent cycle: " + " -> ".join(seen + [cur]))
                break
            seen.append(cur)
            cur = names[cur].get("parent")
    return errors
