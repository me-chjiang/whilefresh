import json
import shutil
import unicodedata
from pathlib import Path

import pytest

from whilefresh_tools.recipes.build import build_document, collect_files, dump, format_report, main
from whilefresh_tools.recipes.fetch import COMMIT
from whilefresh_tools.recipes.parse import clean_name, parse_recipe, recipe_id, source_url

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


def test_output_independent_of_filesystem_order(tmp_path, monkeypatch):
    # Copy the fixtures under NFD-named directories, then feed the files in
    # shuffled order: the output must still be NFC-named and sorted by id.
    for src in DISHES.rglob("*.md"):
        rel = unicodedata.normalize("NFD", src.relative_to(DISHES).as_posix())
        dst = tmp_path / "dishes" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(src.read_bytes())
    dishes = tmp_path / "dishes"
    baseline = dump(build_document(dishes)[0])

    real_rglob = Path.rglob
    monkeypatch.setattr(Path, "rglob", lambda self, pat: list(reversed(sorted(real_rglob(self, pat)))))
    shuffled = dump(build_document(dishes)[0])

    assert shuffled == baseline
    ids = [r["id"] for r in json.loads(baseline)["recipes"]]
    assert ids == sorted(ids) and all(unicodedata.is_normalized("NFC", i) for i in ids)
    assert baseline.endswith("\n") and "西红柿炒鸡蛋" in baseline  # ensure_ascii=False
    assert json.loads(baseline)["recipes"][0]["time_minutes"] is None


def test_cli_writes_lf_only(tmp_path, monkeypatch):
    cache = tmp_path / "c"
    shutil.copytree(DISHES, cache / "dishes")
    (cache / ".complete").write_text("x")
    out = tmp_path / "o.json"
    names = {i["name"] for r in build_document(DISHES)[0]["recipes"] for i in r["ingredients"]}
    ingredient_map = tmp_path / "map.json"
    ingredient_map.write_text(json.dumps({"version": 1, "map": {n: [] for n in names}}))
    args = ["build", "--cache-dir", str(cache), "--out", str(out), "--report", str(tmp_path / "r.md")]
    assert main(args + ["--ingredient-map", str(ingredient_map)]) == 0
    assert b"\r" not in out.read_bytes() and b"\r" not in (tmp_path / "r.md").read_bytes()


def _ingredients(*bullets, steps="1. 做"):
    text = "# 测试的做法\n\n## 必备原料和工具\n\n" + "\n".join(bullets) + f"\n\n## 操作\n\n{steps}\n"
    record, _ = parse_recipe(text, "staple/测试.md")
    return record


def _kinds(record):
    return {i["raw"]: i["kind"] for i in record["ingredients"]}


@pytest.mark.parametrize("food", ["昆布", "韭菜盒子", "藕盒", "茄盒", "糯米纸", "鱼板"])
def test_foods_with_tool_like_names_are_not_tools(food):
    assert _kinds(_ingredients(f"- {food}"))[food] != "tool"


@pytest.mark.parametrize("tool", ["烤箱", "铲子", "保鲜盒", "筷子一双", "隔热手套 1 双", "砂锅或高压锅"])
def test_real_tools_still_detected(tool):
    assert _kinds(_ingredients(f"- {tool}"))[tool] == "tool"


def test_mixed_item_is_judged_per_part():
    k = _kinds(_ingredients("- 油 + 锅 + 菜刀 + 铲子", "- 网购蛋挞液 1 盒，蛋挞皮 1 盒", "- 清水 720g + 600g"))
    assert k["油"] != "tool" and k["锅"] == k["菜刀"] == k["铲子"] == "tool"
    assert "tool" not in (k["网购蛋挞液 1 盒"], k["蛋挞皮 1 盒"])
    assert "清水 720g + 600g" in k  # quantity sum is not split


def test_descriptive_text_goes_to_notes_not_ingredients():
    r = _ingredients("- 盐", "- 炒糖色过程火不要太大！！！电磁炉温度不够，火候过了发苦", "- 这是一段非常非常长的说明文字用来描述怎么选择合适的食材")
    assert [i["name"] for i in r["ingredients"]] == ["盐"]
    assert len(r["ingredient_notes"]) == 2


def test_prefix_optional_applies_to_every_split_piece():
    r = _ingredients("- 可选：盐、糖", "- 柱候酱，叉烧酱（可选），蚝油")
    opt = {i["name"]: i["optional"] for i in r["ingredients"]}
    assert opt == {"盐": True, "糖": True, "柱候酱": False, "叉烧酱": True, "蚝油": False}


def test_ordered_list_ingredients_are_items():
    r = _ingredients("1. 鸡蛋", "2、 西红柿", "3) 盐")
    assert [i["name"] for i in r["ingredients"]] == ["鸡蛋", "西红柿", "盐"]
    assert r["ingredient_notes"] == []


def test_unclosed_bracket_is_dropped():
    assert clean_name("糖（可选") == "糖"
    assert clean_name("炒锅一个(锅太小难炒") == "炒锅一个"
    assert clean_name("盐）") == "盐"
    assert _ingredients("- 糖（可选")["ingredients"][0]["optional"] is True


def test_html_and_reference_images_and_empty_markers_removed():
    steps = "\n".join(
        [
            "1. 切菜",
            '   <img src="./a.png" width="200">',
            "2. 炒菜 ![炒][pic] 好了",
            "![炒][pic]",
            ">",
            "-",
            "> 注意火候",
            "",
            "[pic]: ./pic.png",
            "[keep]: https://example.com",
        ]
    )
    r = _ingredients("- 盐", steps=steps)
    assert r["steps"] == ["1. 切菜", "2. 炒菜  好了", "> 注意火候", "[keep]: https://example.com"]
    assert "pic.png" not in json.dumps(r) and "<img" not in json.dumps(r)


def test_tilde_fence_hides_headings():
    steps = "1. 做法\n~~~\n## 不是标题\n~~~\n2. 完成"
    r = _ingredients("- 盐", steps=steps)
    assert r["steps"][1] == "~~~\n## 不是标题\n~~~" and len(r["steps"]) == 3


def test_report_sections(doc):
    text = format_report(*doc)
    for heading in ("收录总数: 3", "## 各分类数量", "## 跳过的文件", "## 缺节或缺字段的菜谱", "unknown 原料项频次前 50"):
        assert heading in text
