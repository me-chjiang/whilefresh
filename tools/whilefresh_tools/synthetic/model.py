"""Data model of a synthetic sample and its ground-truth JSON.

A sample is described once (products, noise, layout hints); the renderers draw it and
`build_truth` derives the expected recognition output from the same description, so
picture and answer cannot drift apart.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SYNTHETIC_DIR = REPO_ROOT / "test-data" / "synthetic"
RECORDINGS_DIR = REPO_ROOT / "test-data" / "recordings" / "synthetic"
VOCABULARY_PATH = REPO_ROOT / "app" / "assets" / "data" / "vocabulary.json"

UNCATEGORIZED = "未归类"
CATEGORIES = ("food", "seasoning", "irrelevant")
SOURCE_TYPES = ("cart", "order", "receipt", "text", "other")
SAMPLE_KINDS = ("screenshot", "receipt", "text")
SCHEMA_VERSION = 1

# Directory under test-data/synthetic/ and file extension per sample kind.
KIND_DIRS = {"screenshot": "screenshots", "receipt": "receipts", "text": "texts"}
KIND_EXTENSIONS = {"screenshot": ".png", "receipt": ".jpg", "text": ".txt"}

# Noise kinds: things that are on screen but are not purchases.
NOISE_KINDS = (
    "换购推荐",
    "热卖推荐",
    "凑单提示",
    "猜你喜欢",
    "广告横幅",
    "配送费",
    "包装费",
    "优惠券",
    "满减",
)

ITEM_KEYS = (
    "canonical_name",
    "raw_name",
    "quantity_text",
    "count",
    "category",
    "selected",
    "is_gift",
    "production_date_text",
    "expiry_date_text",
    "purchase_date_text",
    "optional",
)


@dataclass(frozen=True)
class Product:
    canonical: str  # standard vocabulary name, or UNCATEGORIZED for non-food
    raw: str  # name exactly as drawn in the sample
    spec: str | None  # quantity / specification text exactly as drawn
    price: float  # unit price in yuan
    count: int = 1
    category: str = "food"
    selected: bool = True
    gift: bool = False
    production: str | None = None
    expiry: str | None = None
    optional: bool = False  # drawn but may be unreadable (occluded); misses are not penalised
    emoji: str | None = None
    orig_price: float | None = None  # struck-through price


@dataclass(frozen=True)
class Rec:
    """A recommended / upsell product: drawn, but not a purchase."""

    name: str
    spec: str
    price: float
    emoji: str = "🛒"


@dataclass(frozen=True)
class Noise:
    kind: str
    title: str = ""  # banner text, fee label, section title
    amount: float | None = None
    recs: tuple[Rec, ...] = ()


@dataclass(frozen=True)
class Group:
    """One store section of a cart, or one order of an order list."""

    name: str
    products: tuple[Product, ...]
    order_time: str | None = None  # purchase time text as drawn
    status: str | None = None


@dataclass(frozen=True)
class Sample:
    id: str
    kind: str  # screenshot | receipt | text
    style: str  # renderer key
    source_type: str  # cart | order | receipt | text | other
    capture_date: date
    groups: tuple[Group, ...]
    noise: tuple[Noise, ...] = ()
    seed: int = 0
    # Style-specific drawing options (min_height, address, eta, order_no, pay, variant ...).
    extra: dict = field(default_factory=dict)

    @property
    def products(self) -> list[Product]:
        return [p for g in self.groups for p in g.products]


def sample_dir(sample: Sample) -> Path:
    return SYNTHETIC_DIR / KIND_DIRS[sample.kind]


def sample_file(sample: Sample) -> Path:
    return sample_dir(sample) / (sample.id + KIND_EXTENSIONS[sample.kind])


def truth_file(sample: Sample) -> Path:
    return sample_dir(sample) / (sample.id + ".json")


def build_truth(sample: Sample) -> dict:
    items = []
    for group in sample.groups:
        for p in group.products:
            items.append(
                {
                    "canonical_name": p.canonical,
                    "raw_name": p.raw,
                    "quantity_text": p.spec,
                    "count": p.count,
                    "category": p.category,
                    "selected": p.selected,
                    "is_gift": p.gift,
                    "production_date_text": p.production,
                    "expiry_date_text": p.expiry,
                    "purchase_date_text": group.order_time,
                    "optional": p.optional,
                }
            )
    ignored_items = []
    for n in sample.noise:
        if n.recs:
            ignored_items.extend({"raw_name": r.name, "kind": n.kind} for r in n.recs)
        elif n.kind in ("配送费", "包装费", "优惠券", "满减"):
            ignored_items.append({"raw_name": n.title, "kind": n.kind})
    kinds = sorted({n.kind for n in sample.noise})
    return {
        "schema_version": SCHEMA_VERSION,
        "id": sample.id,
        "sample_kind": sample.kind,
        "style": sample.style,
        "source_type": sample.source_type,
        "capture_date": sample.capture_date.isoformat(),
        "file": sample_file(sample).name,
        "items": items,
        "ignored_kinds": kinds,
        "ignored_items": ignored_items,
    }


def validate_truth(doc: dict, vocabulary_names: set[str] | None = None) -> list[str]:
    """Schema check of one ground-truth document; returns error strings."""
    errors: list[str] = []

    def need(cond: bool, msg: str) -> None:
        if not cond:
            errors.append(msg)

    for key in ("schema_version", "id", "sample_kind", "style", "source_type", "capture_date", "file",
                "items", "ignored_kinds", "ignored_items"):
        need(key in doc, f"missing key {key}")
    if errors:
        return errors
    need(doc["schema_version"] == SCHEMA_VERSION, "schema_version")
    need(doc["sample_kind"] in SAMPLE_KINDS, "sample_kind")
    need(doc["source_type"] in SOURCE_TYPES, "source_type")
    try:
        date.fromisoformat(doc["capture_date"])
    except (TypeError, ValueError):
        errors.append("capture_date is not an ISO date")
    need(isinstance(doc["items"], list) and doc["items"], "items must be a non-empty list")
    for i, item in enumerate(doc["items"]):
        where = f"items[{i}]"
        need(set(item) == set(ITEM_KEYS), f"{where}: keys {sorted(set(item) ^ set(ITEM_KEYS))}")
        if set(item) != set(ITEM_KEYS):
            continue
        need(isinstance(item["raw_name"], str) and item["raw_name"], f"{where}: raw_name")
        need(item["category"] in CATEGORIES, f"{where}: category")
        need(isinstance(item["count"], int) and item["count"] >= 1, f"{where}: count")
        for flag in ("selected", "is_gift", "optional"):
            need(isinstance(item[flag], bool), f"{where}: {flag} must be bool")
        for key in ("quantity_text", "production_date_text", "expiry_date_text", "purchase_date_text"):
            need(item[key] is None or isinstance(item[key], str), f"{where}: {key}")
        if item["category"] == "irrelevant":
            need(item["canonical_name"] == UNCATEGORIZED, f"{where}: irrelevant item must be {UNCATEGORIZED}")
        elif vocabulary_names is not None:
            need(item["canonical_name"] in vocabulary_names, f"{where}: {item['canonical_name']!r} not in vocabulary")
    need(all(k in NOISE_KINDS for k in doc["ignored_kinds"]), "ignored_kinds has an unknown kind")
    for entry in doc["ignored_items"]:
        need(set(entry) == {"raw_name", "kind"}, "ignored_items entry keys")
    return errors
