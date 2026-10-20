"""Reading the FoodKeeper snapshot and turning one product into shelf-life days.

Rules (see tools/sources/foodkeeper/SOURCE.md): per storage mode take the
"from date of purchase" minimum (``DOP_*_Min``); when the product has no ``DOP_*``
data for that mode, fall back to the unprefixed ``*_Min`` unless it describes
storage after cooking or opening. Only numeric time units convert to days.
"""

import re

from ..vocabulary.schema import load_json
from .schema import SNAPSHOT_PATH

UNIT_DAYS = {"Days": 1, "Weeks": 7, "Months": 30, "Years": 365, "Year": 365}

# (output mode, FoodKeeper column prefix, key in the generated file)
MODES = (
    ("fridge", "Refrigerate", "fridge_days"),
    ("freezer", "Freeze", "freezer_days"),
    ("room", "Pantry", "room_days"),
)
MODE_NAMES = tuple(m for m, _, _ in MODES)
DAYS_KEY = {m: k for m, _, k in MODES}

FROZEN_PURCHASE_CATEGORY = 8  # "Food Purchased Frozen": its Refrigerate_* is the period after thawing/cooking
AFTER_COOKING = re.compile(r"after cooking|after opening|prepared for drinking", re.I)


def to_days(minimum, metric) -> int | None:
    """Days for a ``_Min`` value and its ``_Metric``; None when it is not a plain duration."""
    unit = UNIT_DAYS.get(metric)
    if unit is None or not isinstance(minimum, (int, float)) or isinstance(minimum, bool):
        return None
    days = int(minimum * unit + 0.5)
    return days if days >= 1 else None


def _flatten(row: list[dict]) -> dict:
    out: dict = {}
    for cell in row:
        out.update(cell)
    return out


def load_products(path=SNAPSHOT_PATH) -> dict[int, dict]:
    """Product id -> flattened row (plus ``Category_Name``)."""
    doc = load_json(path)
    sheets = {s["name"]: s["data"] for s in doc["sheets"]}
    categories = {}
    for row in sheets["Category"]:
        cell = _flatten(row)
        categories[int(cell["ID"])] = cell["Category_Name"]
    products: dict[int, dict] = {}
    for row in sheets["Product"]:
        cell = _flatten(row)
        cell["Category_Name"] = categories.get(int(cell["Category_ID"]))
        products[int(cell["ID"])] = cell
    return products


def product_label(product: dict) -> str:
    name = str(product.get("Name") or "").strip()
    sub = str(product.get("Name_subtitle") or "").strip()
    return f"{name} ({sub})" if sub else name


def _present(product: dict, prefix: str) -> bool:
    return product.get(prefix + "_Min") is not None or product.get(prefix + "_Metric") is not None


def mode_days(product: dict, prefix: str) -> tuple[int | None, str | None]:
    """(days, basis column) for one storage mode; (None, None) when nothing usable."""
    dop = "DOP_" + prefix
    if _present(product, dop):
        days = to_days(product.get(dop + "_Min"), product.get(dop + "_Metric"))
        return (days, dop + "_Min") if days else (None, None)
    if not _present(product, prefix):
        return None, None
    if prefix == "Refrigerate":
        tips = str(product.get("Refrigerate_tips") or "")
        if int(product["Category_ID"]) == FROZEN_PURCHASE_CATEGORY or AFTER_COOKING.search(tips):
            return None, None
    days = to_days(product.get(prefix + "_Min"), product.get(prefix + "_Metric"))
    return (days, prefix + "_Min") if days else (None, None)


def product_shelf_life(product: dict) -> dict:
    """{'fridge_days', 'freezer_days', 'room_days'} (None when absent) and 'basis' {mode: column}."""
    result: dict = {}
    basis: dict = {}
    for mode, prefix, key in MODES:
        days, column = mode_days(product, prefix)
        result[key] = days
        if column:
            basis[mode] = column
    result["basis"] = basis
    return result
