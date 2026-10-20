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


def normalize_groups(value) -> list[list[str]]:
    """Requirement groups from a map value.

    Canonical form is a list of groups; a group is a list of alternative standard
    names, any one of which satisfies it, and groups are all required. A bare string
    element is shorthand for a one-name group, so the old flat list ["葱", "姜"]
    means [["葱"], ["姜"]]. Malformed input is returned as-is for validation.
    """
    if not isinstance(value, list):
        return value
    return [[g] if isinstance(g, str) else g for g in value]


def dump_map(doc: dict) -> str:
    """One mapping per line, keys sorted, values as requirement groups."""
    entries = ",\n".join(
        f"  {_line(k)}: {_line(normalize_groups(v))}" for k, v in sorted(doc["map"].items())
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


def validate_map(map_doc: dict, items: list[dict]) -> list[str]:
    """Groups are non-empty, free of repeats, and only name vocabulary items; a key
    that is itself a standard name or alias must require that item."""
    errors: list[str] = []
    mapping = map_doc.get("map")
    if not isinstance(mapping, dict):
        return ["map must be an object"]
    names = {i["name"] for i in items if isinstance(i, dict) and isinstance(i.get("name"), str)}
    owner = {}
    for i in items:
        if isinstance(i, dict) and isinstance(i.get("name"), str):
            owner[norm_key(i["name"])] = i["name"]
            for a in i.get("aliases") or []:
                if isinstance(a, str):
                    owner.setdefault(norm_key(a), i["name"])
    for raw, value in mapping.items():
        groups = normalize_groups(value)
        if not isinstance(groups, list):
            errors.append(f"map[{raw!r}]: value must be a list of requirement groups")
            continue
        flat: list[str] = []
        seen_groups: set[tuple] = set()
        for group in groups:
            if not isinstance(group, list) or not group or not all(isinstance(t, str) for t in group):
                errors.append(f"map[{raw!r}]: bad group {group!r} (must be a non-empty list of names)")
                continue
            if len(set(group)) != len(group):
                errors.append(f"map[{raw!r}]: repeated name in group {group}")
            if tuple(sorted(group)) in seen_groups:
                errors.append(f"map[{raw!r}]: group {group} appears twice")
            seen_groups.add(tuple(sorted(group)))
            for t in group:
                if t not in names:
                    errors.append(f"map[{raw!r}]: target {t!r} is not in the vocabulary")
            flat += group
        std = owner.get(norm_key(raw))
        if std is not None and std not in flat:
            errors.append(f"map[{raw!r}] is the name or alias of {std!r} but does not require it: {groups}")
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
            expected = [] if ing["kind"] == "tool" else normalize_groups(mapping.get(ing["name"]))
            if expected is not None and ing["canonical"] != expected:
                errors.append(
                    f"{r['id']}: canonical of {ing['name']!r} differs from the map"
                )
    return errors


def validate_all(vocab_doc: dict, map_doc: dict, recipes_doc: dict) -> list[str]:
    errors = validate_vocabulary(vocab_doc)
    items = vocab_doc.get("items") if isinstance(vocab_doc.get("items"), list) else []
    return errors + validate_map(map_doc, items) + validate_recipes(recipes_doc, map_doc)
