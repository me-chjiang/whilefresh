import json
import unicodedata
from pathlib import Path

import pytest

from whilefresh_tools.recipes.build import build_document, collect_files, dump, format_report
from whilefresh_tools.recipes.fetch import COMMIT
from whilefresh_tools.recipes.parse import parse_recipe, recipe_id, source_url

DISHES = Path(__file__).parent / "fixtures" / "dishes"


@pytest.fixture(scope="module")
def doc():
    return build_document(DISHES)


@pytest.fixture(scope="module")
def by_id(doc):
    return {r["id"]: r for r in doc[0]["recipes"]}


def test_template_excluded_and_ids_sorted(doc):
    ids = [r["id"] for r in doc[0]["recipes"]]
    assert ids == sorted(ids)
    assert not any("template" in i for i in ids)
    assert not any("template" in p.parts for p in collect_files(DISHES))
    assert doc[1]["skipped"] == []


def test_source_header(doc):
    src = doc[0]["source"]
    assert src["commit"] == COMMIT
    assert src["license"] == "Unlicense"
    assert "generated" not in json.dumps(src)


def test_flat_recipe_fields_optional_and_brackets(by_id):
    r = by_id["vegetable_dish/西红柿炒鸡蛋"]
    assert (r["name"], r["category"], r["category_zh"]) == ("西红柿炒鸡蛋", "vegetable_dish", "素菜")
    assert (r["difficulty"], r["calories_kcal"], r["time_minutes"]) == (2, 252, None)
    sugar = next(i for i in r["ingredients"] if i["name"] == "糖")
    assert sugar == {"raw": "糖（可选）", "name": "糖", "optional": True, "kind": "ingredient"}
    egg = next(i for i in r["ingredients"] if i["name"] == "鸡蛋")
    assert egg["optional"] is False
    assert {"name": "鸡蛋", "expr": "1.5 个 * 份数，向上取整"} in r["quantities"]
    assert r["steps"][1].startswith("2. 可选：去掉西红柿的外表皮\n  - ")
    assert len(r["steps"]) == 11
    assert "`" not in r["summary"] and r["summary"].startswith("一道酸甜")


def test_subdirectory_id_and_url_encoding(by_id):
    r = by_id["meat_dish/香辣鸡爪煲/香辣鸡爪煲"]
    assert r["category"] == "meat_dish"
    assert r["source_url"] == (
        f"https://github.com/Anduin2017/HowToCook/blob/{COMMIT}/dishes/meat_dish/"
        "%E9%A6%99%E8%BE%A3%E9%B8%A1%E7%88%AA%E7%85%B2/%E9%A6%99%E8%BE%A3%E9%B8%A1%E7%88%AA%E7%85%B2.md"
    )
    assert any(i["name"] == "辣椒面" and i["optional"] for i in r["ingredients"])


def test_images_stripped_everywhere(by_id):
    r = by_id["meat_dish/香辣鸡爪煲/香辣鸡爪煲"]
    assert len(r["steps"]) == 8  # the last item is the trailing 注 paragraph
    assert "result1.jpg" not in json.dumps(r, ensure_ascii=False)
    assert r["steps"][2] == "3. 加入香叶、八角、生抽、老抽，盖盖小火慢煮三十分钟。"
    for rec in by_id.values():
        assert "![" not in json.dumps(rec, ensure_ascii=False)


def test_tools_detected_and_missing_quantities_tolerated(doc, by_id):
    r = by_id["dessert/龟苓膏/龟苓膏"]
    kinds = {i["name"]: i["kind"] for i in r["ingredients"]}
    assert kinds["小锅"] == "tool"
    assert kinds["搅拌工具"] == "tool"
    assert kinds["模具或碗"] == "tool"
    assert kinds["开水 500 毫升"] == "unknown"
    assert r["quantities"] == []
    assert len(r["steps"]) == 6
    assert "dessert/龟苓膏/龟苓膏" in doc[1]["issues"]
    assert "missing:quantities" in doc[1]["issues"]["dessert/龟苓膏/龟苓膏"]
    assert "Issue" not in r["notes"]


def test_missing_sections_do_not_crash():
    record, issues = parse_recipe("# 空菜的做法\n\n一句话。\n", "staple/空菜.md")
    assert record["name"] == "空菜"
    assert record["ingredients"] == [] and record["steps"] == [] and record["quantities"] == []
    assert record["difficulty"] is None and record["calories_kcal"] is None
    assert {"missing:ingredients", "missing:steps", "missing:quantities"} <= set(issues)


def test_unusable_file_returns_none():
    record, issues = parse_recipe("随便一些文字\n", "staple/乱.md")
    assert record is None and issues


def test_id_and_url_normalise_unicode():
    nfd = unicodedata.normalize("NFD", "meat_dish/鱼香肉丝.md")
    assert recipe_id(nfd) == "meat_dish/鱼香肉丝"
    assert source_url(nfd) == source_url("meat_dish/鱼香肉丝.md")
    assert source_url("a/b c#d.md").endswith("/dishes/a/b%20c%23d.md")


def test_output_deterministic():
    a = dump(build_document(DISHES)[0])
    b = dump(build_document(DISHES)[0])
    assert a == b
    assert a.endswith("\n") and "西红柿炒鸡蛋" in a  # ensure_ascii=False
    assert json.loads(a)["recipes"][0]["time_minutes"] is None


def test_report_sections(doc):
    text = format_report(*doc)
    for heading in ("收录总数: 3", "## 各分类数量", "## 跳过的文件", "## 缺节或缺字段的菜谱", "unknown 原料项频次前 50"):
        assert heading in text
