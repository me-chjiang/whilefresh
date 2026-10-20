"""Vocabulary / ingredient-map file formats, loading, serialisation and validation."""

import json
from pathlib import Path

PACKAGE_DIR = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[3]
SOURCE_PATH = PACKAGE_DIR / "vocabulary-source.json"
MAP_PATH = PACKAGE_DIR / "ingredient-map.json"
VOCABULARY_OUT = REPO_ROOT / "app" / "assets" / "data" / "vocabulary.json"
RECIPES_PATH = REPO_ROOT / "app" / "assets" / "data" / "recipes.json"

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


def dump_map(doc: dict) -> str:
    """One mapping per line, keys sorted."""
    entries = ",\n".join(
        f"  {_line(k)}: {_line(v)}" for k, v in sorted(doc["map"].items())
    )
    return '{\n "version": %d,\n "map": {\n%s\n }\n}\n' % (doc["version"], entries)


def sorted_items(items: list[dict]) -> list[dict]:
    order = {c: i for i, c in enumerate(CATEGORY_CODES)}
    return sorted(
        (normalize_item(i) for i in items),
        key=lambda i: (order.get(i["category"], 99), i["name"]),
    )


def categories_doc() -> list[dict]:
    return [{"code": c, "name": n} for c, n in CATEGORIES]


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
        names[name] = it
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
            if alias in names:
                errors.append(f"alias {alias!r} of {name!r} collides with a standard name")
            if alias in alias_owner:
                errors.append(
                    f"alias {alias!r} used by both {alias_owner[alias]!r} and {name!r}"
                )
            alias_owner[alias] = name

    for name, it in names.items():
        parent = it.get("parent")
        if parent is None:
            continue
        if parent not in names:
            errors.append(f"{name!r}: parent {parent!r} is not in the vocabulary")
        elif parent == name:
            errors.append(f"{name!r}: parent is itself")
    for name in names:
        seen = [name]
        cur = names[name].get("parent")
        while cur in names:
            if cur in seen:
                errors.append("parent cycle: " + " -> ".join(seen + [cur]))
                break
            seen.append(cur)
            cur = names[cur].get("parent")
    return errors


def validate_map(map_doc: dict, vocab_names: set[str]) -> list[str]:
    errors: list[str] = []
    mapping = map_doc.get("map")
    if not isinstance(mapping, dict):
        return ["map must be an object"]
    for raw, targets in mapping.items():
        if not isinstance(targets, list) or not all(isinstance(t, str) for t in targets):
            errors.append(f"map[{raw!r}]: value must be a list of names")
            continue
        if len(set(targets)) != len(targets):
            errors.append(f"map[{raw!r}]: repeated target in {targets}")
        for t in targets:
            if t not in vocab_names:
                errors.append(f"map[{raw!r}]: target {t!r} is not in the vocabulary")
    return errors


def recipe_ingredient_names(recipes_doc: dict) -> set[str]:
    """Names of non-tool ingredient lines; these must all be in the map."""
    return {
        ing["name"]
        for r in recipes_doc["recipes"]
        for ing in r["ingredients"]
        if ing["kind"] != "tool"
    }


def validate_recipes(recipes_doc: dict, map_doc: dict) -> list[str]:
    errors: list[str] = []
    mapping = map_doc.get("map", {})
    needed = recipe_ingredient_names(recipes_doc)
    for name in sorted(needed - set(mapping)):
        errors.append(f"recipe ingredient {name!r} has no entry in the ingredient map")
    for name in sorted(set(mapping) - needed):
        errors.append(f"ingredient map entry {name!r} is not used by any recipe")
    for r in recipes_doc["recipes"]:
        for ing in r["ingredients"]:
            if "canonical" not in ing:
                errors.append(f"{r['id']}: ingredient {ing['name']!r} lacks canonical")
                continue
            expected = [] if ing["kind"] == "tool" else mapping.get(ing["name"])
            if expected is not None and ing["canonical"] != expected:
                errors.append(
                    f"{r['id']}: canonical of {ing['name']!r} differs from the map"
                )
    return errors


def validate_all(vocab_doc: dict, map_doc: dict, recipes_doc: dict) -> list[str]:
    errors = validate_vocabulary(vocab_doc)
    names = {
        i["name"]
        for i in vocab_doc.get("items", [])
        if isinstance(i, dict) and isinstance(i.get("name"), str)
    }
    return errors + validate_map(map_doc, names) + validate_recipes(recipes_doc, map_doc)
