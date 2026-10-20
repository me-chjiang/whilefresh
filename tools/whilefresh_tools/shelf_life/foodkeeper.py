"""Reading the FoodKeeper snapshot and turning one product into shelf-life days.

Rules (see tools/sources/foodkeeper/SOURCE.md): per storage mode take the
"from date of purchase" minimum (``DOP_*_Min``), or the maximum when the minimum is
no more than SHORT_DAYS days (USDA's 1-2 day floors would make fresh meat, fish and
leafy greens look near expiry on arrival). When the product has no ``DOP_*`` data for
that mode, fall back to the unprefixed columns unless the tips say the figure is for
cooked, opened or otherwise different food. Only numeric time units convert to days.
"""

import re

from ..vocabulary.schema import load_json
from .schema import SNAPSHOT_PATH

UNIT_DAYS = {"Days": 1, "Weeks": 7, "Months": 30, "Years": 365}
SHORT_DAYS = 2

# (output mode, FoodKeeper column prefix, key in the generated file)
MODES = (
    ("fridge", "Refrigerate", "fridge_days"),
    ("freezer", "Freeze", "freezer_days"),
    ("room", "Pantry", "room_days"),
)
DAYS_KEY = {m: k for m, _, k in MODES}
TIPS_COLUMN = {"Pantry": "Pantry_tips", "Refrigerate": "Refrigerate_tips", "Freeze": "Freeze_Tips"}

FROZEN_PURCHASE_CATEGORY = 8  # "Food Purchased Frozen": its Refrigerate_* is the period after thawing/cooking
# Tips saying the generic (unprefixed) figure is for a different state of the food.
OTHER_STATE_TIPS = re.compile(
    r"cooked|mashed|sliced|balled|not recommended|after cooking|after opening|prepared for drinking", re.I
)
RIPE_TIPS = re.compile(r"\bripe\b", re.I)


def to_days(value, metric) -> int | None:
    """Days for a ``_Min`` / ``_Max`` value and its ``_Metric``; None when it is not a plain duration."""
    unit = UNIT_DAYS.get(metric)
    if unit is None or not isinstance(value, (int, float)) or isinstance(value, bool):
        return None
    days = int(value * unit + 0.5)
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


def _days_and_column(product: dict, prefix: str) -> tuple[int | None, str | None]:
    """The minimum in days, or the maximum when the minimum is a 1-2 day floor."""
    metric = product.get(prefix + "_Metric")
    days = to_days(product.get(prefix + "_Min"), metric)
    if not days:
        return None, None
    if days <= SHORT_DAYS:
        longest = to_days(product.get(prefix + "_Max"), metric)
        if longest and longest > days:
            return longest, prefix + "_Max"
    return days, prefix + "_Min"


def mode_days(product: dict, prefix: str) -> tuple[int | None, str | None]:
    """(days, basis column) for one storage mode; (None, None) when nothing usable.

    A generic figure that applies to ripe fruit keeps its value; the column gets a
    ``(ripe)`` suffix so the report can count them.
    """
    dop = "DOP_" + prefix
    if _present(product, dop):
        return _days_and_column(product, dop)
    if not _present(product, prefix):
        return None, None
    tips = str(product.get(TIPS_COLUMN[prefix]) or "")
    if OTHER_STATE_TIPS.search(tips):
        return None, None
    if prefix == "Refrigerate" and int(product["Category_ID"]) == FROZEN_PURCHASE_CATEGORY:
        return None, None
    days, column = _days_and_column(product, prefix)
    if days and RIPE_TIPS.search(tips):
        column += "(ripe)"
    return days, column


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
