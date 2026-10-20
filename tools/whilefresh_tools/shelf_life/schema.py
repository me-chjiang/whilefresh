"""Shelf-life map / reference-table formats, building and validation."""

import json
from pathlib import Path

from ..vocabulary.schema import REPO_ROOT, VOCABULARY_OUT, load_json

PACKAGE_DIR = Path(__file__).resolve().parent
MAP_PATH = PACKAGE_DIR / "foodkeeper-map.json"
SNAPSHOT_PATH = REPO_ROOT / "tools" / "sources" / "foodkeeper" / "foodkeeper-v128.json"
SHELF_LIFE_OUT = REPO_ROOT / "app" / "assets" / "data" / "shelf-life.json"
FOODKEEPER_VERSION = "v128"

MAX_DAYS = 3650  # ten years: longer than any sane shelf life for the foods listed
MAP_ENTRY_KEYS = ("id", "note", "skip", "override", "reason", "confirmed")
MODE_NAMES = ("fridge", "freezer", "room")
DAYS_KEYS = ("fridge_days", "freezer_days", "room_days")
WARNING_CODES = ("freezer_lt_fridge", "room_gt_fridge")
MANUAL = "manual"


def _line(obj) -> str:
    return json.dumps(obj, ensure_ascii=False, separators=(", ", ": "))


def dump_map(doc: dict) -> str:
    """One mapping per line, keys sorted."""
    entries = ",\n".join(f"  {_line(k)}: {_line(v)}" for k, v in sorted(doc["map"].items()))
    return (
        '{\n "version": %d,\n "foodkeeper_version": %s,\n "map": {\n%s\n }\n}\n'
        % (doc["version"], _line(doc["foodkeeper_version"]), entries)
    )


def dump_shelf_life(doc: dict) -> str:
    """One item per line, in vocabulary order (the dict's own order)."""
    body = ",\n".join(f"  {_line(k)}: {_line(v)}" for k, v in doc["items"].items())
    return (
        '{\n "version": %d,\n "source": %s,\n "foodkeeper_version": %s,\n "items": {\n%s\n }\n}\n'
        % (doc["version"], _line(doc["source"]), _line(doc["foodkeeper_version"]), body)
    )


def validate_map(map_doc: dict, vocab_names: set[str], products: dict[int, dict]) -> list[str]:
    errors: list[str] = []
    if map_doc.get("version") != 1:
        errors.append("map version must be 1")
    if map_doc.get("foodkeeper_version") != FOODKEEPER_VERSION:
        errors.append(f"foodkeeper_version must be {FOODKEEPER_VERSION!r}")
    mapping = map_doc.get("map")
    if not isinstance(mapping, dict):
        return errors + ["map must be an object"]
    for name, entry in mapping.items():
        if name not in vocab_names:
            errors.append(f"map key {name!r} is not a standard name in the vocabulary")
        if not isinstance(entry, dict):
            errors.append(f"map[{name!r}]: entry must be an object")
            continue
        extra = set(entry) - set(MAP_ENTRY_KEYS)
        if extra:
            errors.append(f"map[{name!r}]: unknown keys {sorted(extra)}")
        fk_id = entry.get("id")
        if not isinstance(fk_id, int) or isinstance(fk_id, bool) or fk_id not in products:
            errors.append(f"map[{name!r}]: FoodKeeper id {fk_id!r} does not exist in the snapshot")
        if not isinstance(entry.get("note"), str) or not entry["note"].strip():
            errors.append(f"map[{name!r}]: note (why this entry fits) is required")
        skip = entry.get("skip", [])
        if not isinstance(skip, list) or not set(skip) <= set(MODE_NAMES):
            errors.append(f"map[{name!r}]: skip must be a list drawn from {list(MODE_NAMES)}")
        override = entry.get("override", {})
        if not isinstance(override, dict) or not set(override) <= set(MODE_NAMES):
            errors.append(f"map[{name!r}]: override must be an object keyed by {list(MODE_NAMES)}")
        else:
            for mode, days in override.items():
                if not isinstance(days, int) or isinstance(days, bool) or not 1 <= days <= MAX_DAYS:
                    errors.append(f"map[{name!r}]: override {mode} must be an integer in 1..{MAX_DAYS}, got {days!r}")
            if set(override) & set(skip if isinstance(skip, list) else []):
                errors.append(f"map[{name!r}]: a mode cannot be both skipped and overridden")
            if override and (not isinstance(entry.get("reason"), str) or not entry["reason"].strip()):
                errors.append(f"map[{name!r}]: override needs a reason")
            if not override and "reason" in entry:
                errors.append(f"map[{name!r}]: reason without override")
        confirmed = entry.get("confirmed", [])
        if not isinstance(confirmed, list) or not set(confirmed) <= set(WARNING_CODES):
            errors.append(f"map[{name!r}]: confirmed must be a list drawn from {list(WARNING_CODES)}")
    return errors


def build_shelf_life(map_doc: dict, products: dict[int, dict], vocab_items: list[dict]) -> dict:
    """Deterministic: vocabulary order, then the fixed key order of an item."""
    from .foodkeeper import DAYS_KEY, product_shelf_life

    items: dict[str, dict] = {}
    for vocab_item in vocab_items:
        name = vocab_item["name"]
        entry = map_doc["map"].get(name)
        if entry is None:
            continue
        life = product_shelf_life(products[entry["id"]])
        for mode in entry.get("skip", []):
            life[DAYS_KEY[mode]] = None
            life["basis"].pop(mode, None)
        for mode, days in entry.get("override", {}).items():
            life[DAYS_KEY[mode]] = days
            life["basis"][mode] = MANUAL
        if all(life[k] is None for k in DAYS_KEYS):
            continue
        items[name] = {
            "fridge_days": life["fridge_days"],
            "freezer_days": life["freezer_days"],
            "room_days": life["room_days"],
            "source": "foodkeeper",
            "foodkeeper_id": entry["id"],
            "basis": {m: life["basis"][m] for m in MODE_NAMES if m in life["basis"]},
        }
    return {"version": 1, "source": "USDA FoodKeeper", "foodkeeper_version": FOODKEEPER_VERSION, "items": items}


def map_entries_without_values(map_doc: dict, built: dict) -> list[str]:
    """Names kept in the map although FoodKeeper gives them no usable number."""
    return sorted(set(map_doc["map"]) - set(built["items"]))


def validate_shelf_life(doc: dict, vocab_names: set[str], products: dict[int, dict]) -> list[str]:
    errors: list[str] = []
    items = doc.get("items")
    if doc.get("version") != 1 or not isinstance(items, dict) or not items:
        return errors + ["shelf life doc needs version 1 and a non-empty items object"]
    for name, it in items.items():
        if name not in vocab_names:
            errors.append(f"{name!r} is not a standard name in the vocabulary")
        if it.get("source") != "foodkeeper":
            errors.append(f"{name!r}: source must be 'foodkeeper'")
        if it.get("foodkeeper_id") not in products:
            errors.append(f"{name!r}: foodkeeper_id {it.get('foodkeeper_id')!r} does not exist")
        present = []
        for mode, key in zip(MODE_NAMES, DAYS_KEYS):
            days = it.get(key)
            if days is None:
                continue
            present.append(mode)
            if not isinstance(days, int) or isinstance(days, bool) or not 1 <= days <= MAX_DAYS:
                errors.append(f"{name!r}: {key} must be an integer in 1..{MAX_DAYS}, got {days!r}")
        if not present:
            errors.append(f"{name!r}: has no day count at all; leave it out instead")
        if not all(isinstance(c, str) and c for c in it.get("basis", {}).values()):
            errors.append(f"{name!r}: basis values must be non-empty strings")
        if sorted(it.get("basis", {})) != sorted(present):
            errors.append(f"{name!r}: basis modes {sorted(it.get('basis', {}))} do not match day counts {sorted(present)}")
    return errors


def shelf_life_warnings(doc: dict, map_doc: dict) -> list[str]:
    """Plausibility checks that need a human decision; ``confirmed`` in the map silences one."""
    out: list[str] = []
    for name, it in doc["items"].items():
        confirmed = map_doc["map"].get(name, {}).get("confirmed", [])
        fridge, freezer, room = it.get("fridge_days"), it.get("freezer_days"), it.get("room_days")
        if fridge and freezer and freezer < fridge and "freezer_lt_fridge" not in confirmed:
            out.append(f"{name!r}: freezer_days {freezer} is shorter than fridge_days {fridge}")
        if fridge and room and room > fridge and "room_gt_fridge" not in confirmed:
            out.append(f"{name!r}: room_days {room} is longer than fridge_days {fridge}")
    return out


def load_vocabulary_items(path: Path = VOCABULARY_OUT) -> list[dict]:
    return load_json(path)["items"]
