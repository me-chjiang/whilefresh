import copy
import json

import pytest

from whilefresh_tools.shelf_life import draft as draft_mod
from whilefresh_tools.shelf_life.build import main as shelf_main
from whilefresh_tools.shelf_life.foodkeeper import (
    load_products,
    mode_days,
    product_shelf_life,
    to_days,
)
from whilefresh_tools.shelf_life.report import coverage_rows, covering_ancestor, make_report, recipe_counts
from whilefresh_tools.shelf_life.schema import (
    MAP_PATH,
    SHELF_LIFE_OUT,
    build_shelf_life,
    dump_map,
    dump_shelf_life,
    shelf_life_warnings,
    validate_map,
    validate_shelf_life,
)
from whilefresh_tools.vocabulary import deepseek
from whilefresh_tools.vocabulary.schema import RECIPES_PATH, VOCABULARY_OUT, load_json


def product(pid=1, category=7, **cols):
    return {"ID": pid, "Category_ID": category, "Name": f"P{pid}", "Category_Name": "x", **cols}


def vitem(name, category="vegetable", parent=None, pantry=False):
    d = {"name": name, "category": category, "aliases": [], "pantry_default": pantry, "bulk_default": False}
    if parent:
        d["parent"] = parent
    return d


# --- unit conversion -------------------------------------------------------------

@pytest.mark.parametrize(
    "value,metric,expected",
    [
        (3.0, "Days", 3),
        (2.0, "Weeks", 14),
        (6.0, "Months", 180),
        (2.0, "Years", 730),
        (1.0, "Year", 365),
        (1.5, "Weeks", 11),
        (None, "Days", None),
        (3.0, None, None),
        (3.0, "Hours", None),
        (3.0, "When Ripe", None),
        (3.0, "Package use-by date", None),
        (3.0, "Indefinitely", None),
        (3.0, "Not Recommended", None),
        (0.0, "Days", None),
    ],
)
def test_to_days(value, metric, expected):
    assert to_days(value, metric) == expected


# --- field selection ---------------------------------------------------------------

def test_dop_wins_over_generic_field():
    p = product(DOP_Refrigerate_Min=1.0, DOP_Refrigerate_Max=2.0, DOP_Refrigerate_Metric="Weeks",
                Refrigerate_Min=5.0, Refrigerate_Metric="Days")
    assert mode_days(p, "Refrigerate") == (7, "DOP_Refrigerate_Min")


def test_generic_field_used_when_no_dop_data():
    p = product(Refrigerate_Min=2.0, Refrigerate_Max=5.0, Refrigerate_Metric="Days")
    assert mode_days(p, "Refrigerate") == (2, "Refrigerate_Min")


def test_non_numeric_dop_blocks_the_generic_fallback():
    p = product(DOP_Pantry_Metric="Indefinitely", Pantry_Min=6.0, Pantry_Metric="Months")
    assert mode_days(p, "Pantry") == (None, None)


@pytest.mark.parametrize("metric", ["When Ripe", "Not Recommended", "Package use-by date", "Hours"])
def test_non_duration_metrics_leave_the_mode_empty(metric):
    assert mode_days(product(Pantry_Min=3.0, Pantry_Metric=metric), "Pantry") == (None, None)


def test_generic_refrigerate_after_cooking_is_not_used():
    p = product(Refrigerate_Min=3.0, Refrigerate_Metric="Days", Refrigerate_tips="Recommendation is for storage after cooking.")
    assert mode_days(p, "Refrigerate") == (None, None)
    # the same tip text does not matter for freezing
    q = product(Freeze_Min=3.0, Freeze_Metric="Months", Freeze_Tips="Freezing guidance applies after cooking.")
    assert mode_days(q, "Freeze") == (90, "Freeze_Min")


def test_frozen_purchase_category_refrigerate_is_after_thawing():
    p = product(category=8, Refrigerate_Min=3.0, Refrigerate_Metric="Days", Freeze_Min=1.0, Freeze_Metric="Years")
    life = product_shelf_life(p)
    assert life["fridge_days"] is None and life["freezer_days"] == 365
    assert life["basis"] == {"freezer": "Freeze_Min"}


def test_missing_min_is_not_guessed_from_max():
    assert mode_days(product(Pantry_Max=18.0, Pantry_Metric="Months"), "Pantry") == (None, None)


def test_product_shelf_life_shape():
    p = product(DOP_Refrigerate_Min=1.0, DOP_Refrigerate_Metric="Months", DOP_Freeze_Min=6.0, DOP_Freeze_Metric="Months")
    assert product_shelf_life(p) == {
        "fridge_days": 30, "freezer_days": 180, "room_days": None,
        "basis": {"fridge": "DOP_Refrigerate_Min", "freezer": "DOP_Freeze_Min"},
    }


def test_snapshot_loads_and_matches_known_rows():
    products = load_products()
    assert len(products) == 661
    assert products[281]["Name"] == "Celery"
    assert product_shelf_life(products[281])["fridge_days"] == 7
    assert products[281]["Category_Name"] == "Produce"


# --- map validation and building -------------------------------------------------------

PRODUCTS = {
    1: product(1, DOP_Refrigerate_Min=1.0, DOP_Refrigerate_Metric="Weeks", DOP_Freeze_Min=2.0, DOP_Freeze_Metric="Months"),
    2: product(2, Pantry_Min=1.0, Pantry_Metric="Years"),
    3: product(3, Pantry_Metric="Indefinitely"),
    4: product(4, DOP_Refrigerate_Min=10.0, DOP_Refrigerate_Metric="Days", DOP_Freeze_Min=3.0, DOP_Freeze_Metric="Days"),
}
VOCAB = [vitem("甲"), vitem("乙", "staple"), vitem("丙"), vitem("丁")]


def good_map(**entries):
    return {"version": 1, "foodkeeper_version": "v128", "map": entries}


def test_build_is_ordered_and_skips_names_without_values():
    doc = build_shelf_life(good_map(乙={"id": 2, "note": "n"}, 甲={"id": 1, "note": "n"}, 丙={"id": 3, "note": "n"}), PRODUCTS, VOCAB)
    assert list(doc["items"]) == ["甲", "乙"]  # vocabulary order; 丙 has no usable number
    assert doc["items"]["甲"] == {
        "fridge_days": 7, "freezer_days": 60, "room_days": None, "source": "foodkeeper", "foodkeeper_id": 1,
        "basis": {"fridge": "DOP_Refrigerate_Min", "freezer": "DOP_Freeze_Min"},
    }
    assert doc["items"]["乙"]["basis"] == {"room": "Pantry_Min"}
    assert list(doc["items"]["甲"]) == ["fridge_days", "freezer_days", "room_days", "source", "foodkeeper_id", "basis"]


def test_skip_removes_a_mode_and_its_basis():
    doc = build_shelf_life(good_map(甲={"id": 1, "note": "n", "skip": ["freezer"]}), PRODUCTS, VOCAB)
    item = doc["items"]["甲"]
    assert item["freezer_days"] is None and item["basis"] == {"fridge": "DOP_Refrigerate_Min"}


def test_validate_map_catches_bad_entries():
    names = {v["name"] for v in VOCAB}
    assert validate_map(good_map(甲={"id": 1, "note": "n"}), names, PRODUCTS) == []
    bad = good_map(
        不存在={"id": 1, "note": "n"},
        乙={"id": 999, "note": "n"},
        丙={"id": 1, "note": ""},
        丁={"id": 1, "note": "n", "skip": ["attic"], "confirmed": ["x"], "extra": 1},
    )
    text = "\n".join(validate_map(bad, names, PRODUCTS))
    assert "'不存在'" in text and "id 999" in text and "note" in text
    assert "skip must be" in text and "confirmed must be" in text and "unknown keys" in text
    assert validate_map({"version": 2, "foodkeeper_version": "v1", "map": {}}, names, PRODUCTS)
    assert validate_map({"version": 1, "foodkeeper_version": "v128", "map": []}, names, PRODUCTS)


def test_validate_shelf_life_rules():
    names = {v["name"] for v in VOCAB}
    doc = build_shelf_life(good_map(甲={"id": 1, "note": "n"}, 乙={"id": 2, "note": "n"}), PRODUCTS, VOCAB)
    assert validate_shelf_life(doc, names, PRODUCTS) == []

    def broken(mutate):
        d = copy.deepcopy(doc)
        mutate(d["items"])
        return "\n".join(validate_shelf_life(d, names, PRODUCTS))

    assert "not a standard name" in broken(lambda i: i.__setitem__("戊", i["甲"]))
    assert "1..3650" in broken(lambda i: i["甲"].__setitem__("fridge_days", 0))
    assert "1..3650" in broken(lambda i: i["甲"].__setitem__("fridge_days", 4000))
    assert "1..3650" in broken(lambda i: i["甲"].__setitem__("freezer_days", 2.5))
    assert "foodkeeper_id" in broken(lambda i: i["甲"].__setitem__("foodkeeper_id", 404))
    assert "source" in broken(lambda i: i["甲"].__setitem__("source", "ai"))
    assert "basis" in broken(lambda i: i["甲"].__setitem__("basis", {}))
    assert "no day count" in broken(lambda i: i["乙"].__setitem__("room_days", None))


def test_freezer_shorter_than_fridge_warns_until_confirmed():
    m = good_map(丁={"id": 4, "note": "n"})
    doc = build_shelf_life(m, PRODUCTS, VOCAB)
    warnings = shelf_life_warnings(doc, m)
    assert len(warnings) == 1 and "'丁'" in warnings[0]
    m["map"]["丁"]["confirmed"] = ["freezer_lt_fridge"]
    assert shelf_life_warnings(doc, m) == []


# --- the committed data ------------------------------------------------------------------

def test_committed_map_and_table_are_consistent():
    vocab = load_json(VOCABULARY_OUT)["items"]
    products = load_products()
    map_doc = load_json(MAP_PATH)
    assert validate_map(map_doc, {v["name"] for v in vocab}, products) == []
    built = build_shelf_life(map_doc, products, vocab)
    assert set(built["items"]) == set(map_doc["map"]), "every map entry must yield at least one number"
    assert validate_shelf_life(built, {v["name"] for v in vocab}, products) == []
    assert shelf_life_warnings(built, map_doc) == []
    assert SHELF_LIFE_OUT.read_text(encoding="utf-8") == dump_shelf_life(built)
    assert MAP_PATH.read_text(encoding="utf-8") == dump_map(map_doc)


def test_committed_map_keeps_chinese_specialities_blank():
    items = load_json(SHELF_LIFE_OUT)["items"]
    for name in ("腊肉", "豆腐干", "腐竹", "速冻水饺", "香肠", "皮蛋", "咸蛋"):
        assert name not in items


# --- reproducibility ----------------------------------------------------------------------

def test_build_twice_is_byte_identical(tmp_path):
    a, b = tmp_path / "a.json", tmp_path / "b.json"
    assert shelf_main(["build", "--out", str(a)]) == 0
    assert shelf_main(["build", "--out", str(b)]) == 0
    assert a.read_bytes() == b.read_bytes() == SHELF_LIFE_OUT.read_bytes()


def test_validate_command_detects_a_stale_table(tmp_path, capsys):
    stale = tmp_path / "stale.json"
    stale.write_text(SHELF_LIFE_OUT.read_text(encoding="utf-8").replace('"fridge_days": 2,', '"fridge_days": 3,', 1), encoding="utf-8")
    assert shelf_main(["validate", "--out", str(stale)]) == 1
    assert "stale" in capsys.readouterr().err
    assert shelf_main(["validate"]) == 0


def test_build_refuses_an_unknown_id(tmp_path, capsys):
    m = tmp_path / "map.json"
    m.write_text(dump_map(good_map(甲={"id": 99999, "note": "n"})), encoding="utf-8")
    assert shelf_main(["build", "--map", str(m), "--out", str(tmp_path / "o.json")]) == 1
    assert not (tmp_path / "o.json").exists()


def test_no_network_in_build_path(monkeypatch, tmp_path):
    monkeypatch.setattr(deepseek.urllib.request, "urlopen", lambda *a, **k: pytest.fail("network used"))
    monkeypatch.setattr(deepseek, "read_api_key", lambda: pytest.fail("keychain used"))
    assert shelf_main(["build", "--out", str(tmp_path / "o.json")]) == 0


# --- report -----------------------------------------------------------------------------------

def test_coverage_and_fallback():
    vocab = {"items": [vitem("猪肉", "meat"), vitem("五花肉", "meat", parent="猪肉"), vitem("腊肉", "processed")]}
    shelf = {"items": {"猪肉": {}}}
    by_name = {i["name"]: i for i in vocab["items"]}
    assert covering_ancestor("五花肉", by_name, {"猪肉"}) == "猪肉"
    assert covering_ancestor("腊肉", by_name, {"猪肉"}) is None
    rows = {r[0]: r for r in coverage_rows(vocab, shelf)}
    assert rows["meat"][2:] == (2, 1, 2)
    assert rows[""][2:] == (3, 1, 2)


def test_recipe_counts_once_per_recipe():
    recipes = {"recipes": [
        {"ingredients": [{"canonical": [["葱"], ["姜", "蒜"]]}, {"canonical": [["葱"]]}]},
        {"ingredients": [{"canonical": [["葱"]]}, {"canonical": []}]},
    ]}
    counts = recipe_counts(recipes)
    assert counts["葱"] == 2 and counts["姜"] == 1


def test_report_is_deterministic_and_has_all_sections():
    args = (load_json(VOCABULARY_OUT), load_json(RECIPES_PATH), load_json(SHELF_LIFE_OUT), load_json(MAP_PATH), load_products())
    first = make_report(*args)
    assert first == make_report(*args)
    for heading in ("### 覆盖率", "### 无参考值的高频食材", "### 抽样对照", "### 校验警告"):
        assert heading in first
    assert first.count("\n| ") >= 40


# --- DeepSeek draft (mocked) ----------------------------------------------------------------------

class Scripted:
    """Stands in for the DeepSeek client: answers from a function, records requests."""

    def __init__(self, answer):
        self.answer = answer
        self.calls = []

    def complete_json(self, system, user, max_tokens=8000):
        self.calls.append((system, user))
        return self.answer(user)


def test_draft_prompt_lists_candidates_and_names():
    names = [vitem("甲"), {**vitem("乙"), "aliases": ["别名一", "别名二"]}]
    text = draft_mod.user_message(draft_mod.candidate_text(PRODUCTS), names)
    assert "1|P1||x" in text and "2|P2||x" in text
    assert "|P3|" not in text  # an entry with no usable number cannot back a map row
    assert "乙（别名一、别名二）" in text and "标准食材名（2 个）" in text
    assert "null" in draft_mod.SYSTEM and "json" in draft_mod.SYSTEM


def test_run_draft_filters_invalid_ids_and_batches(monkeypatch):
    monkeypatch.setattr(draft_mod, "BATCH_SIZE", 2)
    vocab = [vitem("甲"), vitem("乙"), vitem("丙")]
    replies = {
        "甲": {"id": 1, "reason": "同类"},
        "乙": {"id": 3, "reason": "无数值"},  # entry 3 has no usable number: not a valid candidate
        "丙": {"id": None, "reason": "无对应"},
    }
    client = Scripted(lambda user: {"mappings": {k: v for k, v in replies.items() if f"\n{k}" in user.split("标准食材名")[1]}})
    entries, log = draft_mod.run_draft(client, PRODUCTS, vocab)
    assert len(client.calls) == 2
    assert entries == {"甲": {"id": 1, "note": "同类"}}
    assert any("dropped invalid id" in line for line in log) and any("null" in line for line in log)


def test_run_draft_rejects_a_response_without_mappings():
    with pytest.raises(ValueError):
        draft_mod.run_draft(Scripted(lambda user: {"oops": 1}), PRODUCTS, [vitem("甲")])


def test_draft_uses_the_shared_client_request_and_cache(tmp_path):
    calls = []

    def post(payload, key):
        calls.append((payload, key))
        content = json.dumps({"mappings": {"甲": {"id": 1, "reason": "r"}}})
        return {"choices": [{"message": {"content": content}, "finish_reason": "stop"}],
                "usage": {"prompt_tokens": 1, "completion_tokens": 1, "total_tokens": 2}}

    def run():
        c = deepseek.Client(tmp_path, post=post, key_provider=lambda: "SECRET")
        return c, draft_mod.run_draft(c, PRODUCTS, [vitem("甲")])

    first, (entries, _) = run()
    assert entries == {"甲": {"id": 1, "note": "r"}}
    payload, key = calls[0]
    assert key == "SECRET" and "SECRET" not in json.dumps(payload)
    assert payload["thinking"] == {"type": "disabled"} and payload["response_format"] == {"type": "json_object"}
    assert payload["messages"][0]["content"] == draft_mod.SYSTEM and "甲" in payload["messages"][1]["content"]

    second, (again, _) = run()
    assert again == entries and len(calls) == 1 and second.cache_hits == 1
    assert all("SECRET" not in p.read_text() for p in tmp_path.iterdir())


def test_draft_default_cache_dir_is_separate_from_the_vocabulary_one():
    from whilefresh_tools.shelf_life.build import default_cache_dir

    assert default_cache_dir().name == "shelf-life"
