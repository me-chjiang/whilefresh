import copy
import json
from pathlib import Path

import pytest

from whilefresh_tools.recipes.build import build_document, dump, main as recipes_main
from whilefresh_tools.vocabulary import deepseek
from whilefresh_tools.vocabulary.build import build_vocabulary, main as vocab_main
from whilefresh_tools.vocabulary.draft import Vocab, draft_map, draft_vocabulary, TOPICS
from whilefresh_tools.vocabulary.mapping import MissingMapping, annotate_recipes
from whilefresh_tools.vocabulary.report import make_report
from whilefresh_tools.vocabulary.schema import (
    CATEGORY_CODES,
    MAP_PATH,
    RECIPES_PATH,
    SOURCE_PATH,
    VOCABULARY_OUT,
    categories_doc,
    dump_map,
    dump_vocabulary,
    load_json,
    normalize_groups,
    validate_all,
    validate_map,
    validate_recipes,
    validate_vocabulary,
)

DISHES = Path(__file__).parent / "fixtures" / "dishes"


def item(name, category="vegetable", aliases=(), parent=None, pantry=False, bulk=False):
    d = {"name": name, "category": category, "aliases": list(aliases),
         "pantry_default": pantry, "bulk_default": bulk}
    if parent:
        d["parent"] = parent
    return d


def vocab_doc(*items):
    return {"version": 1, "categories": categories_doc(), "items": list(items)}


GOOD = vocab_doc(
    item("猪肉", "meat"),
    item("五花肉", "meat", ["三层肉"], parent="猪肉"),
    item("西红柿", aliases=["番茄"]),
    item("盐", "seasoning", pantry=True),
)


def test_valid_vocabulary_passes():
    assert validate_vocabulary(GOOD) == []


def _errors(mutate):
    doc = copy.deepcopy(GOOD)
    mutate(doc)
    return validate_vocabulary(doc)


def test_duplicate_standard_name():
    assert any("duplicate" in e for e in _errors(lambda d: d["items"].append(item("盐", "seasoning"))))


def test_alias_collides_with_name_or_other_alias():
    assert any("collides" in e for e in _errors(lambda d: d["items"][2]["aliases"].append("盐")))
    errs = _errors(lambda d: d["items"][0]["aliases"].append("番茄"))
    assert any("used by both" in e for e in errs)


def test_parent_must_exist_and_not_cycle():
    assert any("not in the vocabulary" in e for e in _errors(lambda d: d["items"][1].update(parent="牛肉")))
    assert any("itself" in e for e in _errors(lambda d: d["items"][0].update(parent="猪肉")))

    def cycle(d):
        d["items"][0]["parent"] = "五花肉"

    assert any("cycle" in e for e in _errors(cycle))


def test_category_flags_and_categories_list():
    assert any("invalid category" in e for e in _errors(lambda d: d["items"][0].update(category="meatt")))
    assert any("boolean" in e for e in _errors(lambda d: d["items"][0].update(pantry_default="yes")))
    assert any("categories" in e for e in _errors(lambda d: d.update(categories=[])))
    assert any("unknown keys" in e for e in _errors(lambda d: d["items"][0].update(extra=1)))


def test_map_groups_validation():
    items = GOOD["items"]
    ok = {"map": {"葱姜蒜": [["盐"], ["西红柿"]], "猪肉末": ["猪肉"], "牛奶或奶粉": [["猪肉", "西红柿"]], "冰": []}}
    assert validate_map(ok, items) == []
    assert any("not in the vocabulary" in e for e in validate_map({"map": {"x": [["牛肉"]]}}, items))
    assert any("repeated" in e for e in validate_map({"map": {"x": [["盐", "盐"]]}}, items))
    assert any("bad group" in e for e in validate_map({"map": {"x": [[]]}}, items))
    assert any("bad group" in e for e in validate_map({"map": {"x": [["盐"], [1]]}}, items))
    assert any("twice" in e for e in validate_map({"map": {"x": [["盐", "猪肉"], ["猪肉", "盐"]]}}, items))


def test_map_key_that_is_a_name_or_alias_must_require_it():
    items = GOOD["items"]
    assert validate_map({"map": {"番茄": [["西红柿"]], "西红柿": [["西红柿", "盐"]]}}, items) == []
    errs = validate_map({"map": {"番茄": [["盐"]]}}, items)
    assert any("alias of '西红柿'" in e or "name or alias of '西红柿'" in e for e in errs)
    assert any("name or alias" in e for e in validate_map({"map": {"盐": []}}, items))


def test_groups_shorthand_is_normalised():
    assert normalize_groups(["葱", "姜"]) == [["葱"], ["姜"]]
    assert normalize_groups([["牛奶", "奶粉"], "盐"]) == [["牛奶", "奶粉"], ["盐"]]
    assert normalize_groups([]) == []
    assert json.loads(dump_map({"version": 1, "map": {"葱姜": ["葱", "姜"]}}))["map"] == {"葱姜": [["葱"], ["姜"]]}


def test_alias_identity_ignores_case_and_inner_spaces():
    errs = _errors(lambda d: d["items"].append(item("XO酱", "seasoning", ["XO 酱"], pantry=False)))
    assert any("collides" in e for e in errs)
    errs = _errors(lambda d: d["items"][2]["aliases"].append("Fan Qie"))
    assert not errs

    def clash(d):
        d["items"][2]["aliases"].append("Fan Qie")
        d["items"][3]["aliases"].append("fanqie")

    assert any("used by both" in e for e in _errors(clash))


def test_parent_type_is_checked_before_use():
    assert any("must be a string" in e for e in _errors(lambda d: d["items"][1].update(parent=["猪肉"])))
    assert any("must be a string" in e for e in _errors(lambda d: d["items"][1].update(parent=5)))


def recipes_doc():
    return {
        "recipes": [
            {
                "id": "a/b",
                "ingredients": [
                    {"name": "盐", "kind": "ingredient", "canonical": [["盐"]]},
                    {"name": "水", "kind": "unknown", "canonical": []},
                    {"name": "锅", "kind": "tool", "canonical": []},
                ],
            }
        ]
    }


def test_recipe_coverage_rules():
    mp = {"map": {"盐": ["盐"], "水": []}}
    assert validate_recipes(recipes_doc(), mp) == []
    assert validate_recipes(recipes_doc(), {"map": {"盐": ["盐"], "水": []}}) == []  # flat shorthand is equivalent
    assert any("no entry" in e for e in validate_recipes(recipes_doc(), {"map": {"盐": ["盐"]}}))
    assert any("not used" in e for e in validate_recipes(recipes_doc(), {"map": {**mp["map"], "糖": []}}))
    bad = recipes_doc()
    bad["recipes"][0]["ingredients"][0]["canonical"] = [["糖"]]
    assert any("differs" in e for e in validate_recipes(bad, mp))
    nocanon = recipes_doc()
    del nocanon["recipes"][0]["ingredients"][1]["canonical"]
    assert any("lacks canonical" in e for e in validate_recipes(nocanon, mp))


def test_validate_all_combines():
    assert validate_all(GOOD, {"map": {"盐": ["盐"], "水": []}}, recipes_doc()) == []
    errs = validate_all(GOOD, {"map": {"盐": ["盐"], "水": [["水", "盐"]]}}, recipes_doc())
    assert any("'水'" in e and "not in the vocabulary" in e for e in errs)


def test_annotate_recipes():
    doc = {"recipes": [{"ingredients": [
        {"name": "葱姜蒜", "kind": "unknown"},
        {"name": "锅", "kind": "tool"},
        {"name": "冰块", "kind": "ingredient"},
    ]}]}
    annotate_recipes(doc, {"葱姜蒜": ["葱", "姜", "蒜"], "冰块": []})
    assert [i["canonical"] for i in doc["recipes"][0]["ingredients"]] == [[["葱"], ["姜"], ["蒜"]], [], []]
    alt = {"recipes": [{"ingredients": [{"name": "牛奶或奶粉", "kind": "ingredient"}]}]}
    annotate_recipes(alt, {"牛奶或奶粉": [["牛奶", "奶粉"]]})
    assert alt["recipes"][0]["ingredients"][0]["canonical"] == [["牛奶", "奶粉"]]
    with pytest.raises(MissingMapping, match="糖"):
        annotate_recipes({"recipes": [{"ingredients": [{"name": "糖", "kind": "unknown"}]}]}, {})


def test_recipes_build_adds_canonical_and_is_reproducible():
    doc0, _ = build_document(DISHES)
    names = {i["name"] for r in doc0["recipes"] for i in r["ingredients"] if i["kind"] != "tool"}
    mapping = {n: [n] for n in names}
    first = dump(build_document(DISHES, ingredient_map=mapping)[0])
    second = dump(build_document(DISHES, ingredient_map=mapping)[0])
    assert first == second
    for r in json.loads(first)["recipes"]:
        for ing in r["ingredients"]:
            assert ing["canonical"] == ([] if ing["kind"] == "tool" else [[ing["name"]]])
    with pytest.raises(MissingMapping):
        build_document(DISHES, ingredient_map={})


def test_recipes_cli_fails_cleanly_on_missing_map(tmp_path, capsys):
    import shutil

    cache = tmp_path / "c"
    shutil.copytree(DISHES, cache / "dishes")
    (cache / ".complete").write_text("x")
    empty = tmp_path / "map.json"
    empty.write_text('{"version": 1, "map": {}}')
    rc = recipes_main(["build", "--cache-dir", str(cache), "--out", str(tmp_path / "o.json"),
                       "--report", str(tmp_path / "r.md"), "--ingredient-map", str(empty)])
    assert rc == 1 and "missing from the ingredient map" in capsys.readouterr().err
    assert not (tmp_path / "o.json").exists()


def test_dump_formats_are_stable_and_parse_back():
    text = dump_vocabulary(build_vocabulary(GOOD))
    assert json.loads(text) == build_vocabulary(GOOD)
    assert text.count("\n") == len(GOOD["items"]) + 6 and text.endswith("\n")
    assert text == dump_vocabulary(build_vocabulary(json.loads(text)))
    m = {"version": 1, "map": {"b": [["x"]], "a": []}}
    assert json.loads(dump_map(m)) == m and dump_map(m).index('"a"') < dump_map(m).index('"b"')


def test_build_sorts_by_category_then_name():
    doc = build_vocabulary(vocab_doc(item("盐", "seasoning"), item("白菜"), item("菠菜")))
    assert [i["name"] for i in doc["items"]] == sorted(["白菜", "菠菜"]) + ["盐"]
    assert list(doc["items"][0]) == ["name", "category", "aliases", "pantry_default", "bulk_default"]


# --- committed artefacts -----------------------------------------------------


def test_committed_data_is_valid_and_consistent():
    vocab = load_json(VOCABULARY_OUT)
    assert validate_all(vocab, load_json(MAP_PATH), load_json(RECIPES_PATH)) == []
    assert vocab == build_vocabulary(load_json(SOURCE_PATH))
    assert VOCABULARY_OUT.read_text(encoding="utf-8") == dump_vocabulary(vocab)
    assert dump_map(load_json(MAP_PATH)) == MAP_PATH.read_text(encoding="utf-8")
    assert [c["code"] for c in vocab["categories"]] == list(CATEGORY_CODES)


def test_committed_vocabulary_contract_for_other_modules():
    items = {i["name"]: i for i in load_json(VOCABULARY_OUT)["items"]}
    alias = {a: i["name"] for i in items.values() for a in i["aliases"]}
    assert {"五花肉": "猪肉", "牛腩": "牛肉", "小葱": "葱"} == {k: items[k]["parent"] for k in ("五花肉", "牛腩", "小葱")}
    for pantry in ("盐", "食用油", "生抽", "老抽", "白糖", "醋", "料酒", "大米", "葱", "姜", "蒜", "水"):
        assert items[pantry]["pantry_default"], pantry
    assert not items["鸡蛋"]["pantry_default"] and items["鸡蛋"]["bulk_default"]
    assert items["生抽"] is not items["老抽"] and items["生抽"]["parent"] == items["老抽"]["parent"] == "酱油"
    cart = {"麻辣香肠": "香肠", "牛肉火锅片": "肥牛", "掌中宝": "鸡脆骨", "羊肉火锅片": "羊肉卷", "猪肋排": "排骨",
            "长尖椒": "尖椒", "白蘑菇": "蘑菇", "什锦沙拉菜": "沙拉菜", "铁棍山药": "山药", "番茄": "西红柿", "米": "大米"}
    for term, std in cart.items():
        assert alias[term] == std
    for name in ("鸡蛋", "牛排", "茭白", "蓝莓", "柿子"):
        assert name in items
    assert 400 <= len(items) <= 700


PANTRY_WHITELIST = {
    "盐", "白糖", "食用油", "酱油", "生抽", "老抽", "醋", "陈醋", "香醋", "米醋", "白醋", "料酒", "大米", "葱", "大葱", "小葱",
    "姜", "蒜", "水", "蚝油", "胡椒粉", "淀粉", "鸡精", "菜籽油", "花生油", "大豆油", "玉米油", "葵花籽油", "玉米淀粉",
    "土豆淀粉", "红薯淀粉", "黑胡椒", "白胡椒", "味精", "芝麻香油",
}
NO_PARENT = ("排骨", "猪肘", "猪肉末", "羊蝎子", "整鸡", "奶油奶酪", "马斯卡彭芝士", "青椒", "梅花肉", "猪颈肉", "猪腱子")


def test_pantry_defaults_equal_the_reviewed_whitelist():
    pantry = {i["name"] for i in load_json(VOCABULARY_OUT)["items"] if i["pantry_default"]}
    assert pantry == PANTRY_WHITELIST
    assert "橄榄油" not in pantry


def test_parts_do_not_satisfy_their_whole_and_reviewed_parents_hold():
    items = {i["name"]: i for i in load_json(VOCABULARY_OUT)["items"]}
    for name in NO_PARENT:
        assert "parent" not in items[name], name
    assert items["鸡骨架"]["category"] == "meat" and "parent" not in items["鸡骨架"]
    assert {"鸡架", "鸡壳"} <= set(items["鸡骨架"]["aliases"])
    assert (items["干木耳"]["parent"], items["干香菇"]["parent"], items["木薯粉"]["parent"]) == ("木耳", "香菇", "淀粉")


def test_reviewed_alias_moves():
    items = {i["name"]: i for i in load_json(VOCABULARY_OUT)["items"]}
    owner = {a: i["name"] for i in items.values() for a in i["aliases"]}
    moved = {"姜片": "姜", "生菜叶": "生菜", "海米": "虾米", "沙姜": "山奈", "甜椒": "彩椒", "奶酪片": "芝士片"}
    for alias, std in moved.items():
        assert owner[alias] == std
    for alias in ("甘蓝菜", "米酒", "甜酒", "臭菜", "龙须菜", "苦菜", "花鲫鱼", "刀鱼", "孜然茴香", "草豆蔻"):
        assert alias not in owner, alias


def test_committed_map_uses_requirement_groups():
    mapping = load_json(MAP_PATH)["map"]
    assert all(isinstance(g, list) and g and all(isinstance(t, str) for t in g) for v in mapping.values() for g in v)
    assert mapping["葱姜蒜"] == [["葱"], ["姜"], ["蒜"]]
    assert mapping["牛奶或淡奶油或炼乳或奶粉"] == [["牛奶", "淡奶油", "炼乳", "奶粉"]]
    assert mapping["冰块"] == []
    assert len(mapping["菌菇"][0]) >= 3 and len(mapping["菌菇"]) == 1
    for r in load_json(RECIPES_PATH)["recipes"]:
        for ing in r["ingredients"]:
            assert ing["canonical"] == ([] if ing["kind"] == "tool" else normalize_groups(mapping[ing["name"]]))


def test_cli_validate_and_build_roundtrip(tmp_path, capsys):
    assert vocab_main(["validate"]) == 0
    out = tmp_path / "v.json"
    assert vocab_main(["build", "--out", str(out)]) == 0
    assert out.read_bytes() == VOCABULARY_OUT.read_bytes()
    capsys.readouterr()


def test_report_is_reproducible():
    args = (load_json(VOCABULARY_OUT), load_json(MAP_PATH), load_json(RECIPES_PATH), 60, 7)
    a, b = make_report(*args), make_report(*args)
    assert a == b and "标准名总数" in a and a.count("\n| ") >= 60


# --- DeepSeek client (no network) ----------------------------------------------


def fake_reply(content, finish="stop"):
    return {"choices": [{"message": {"content": json.dumps(content)}, "finish_reason": finish}],
            "usage": {"prompt_tokens": 10, "completion_tokens": 5, "total_tokens": 15}}


def test_request_payload_and_cache(tmp_path):
    calls = []

    def post(payload, key):
        calls.append((payload, key))
        return fake_reply({"items": []})

    c = deepseek.Client(tmp_path, post=post, key_provider=lambda: "SECRET")
    assert c.complete_json("sys json", "user") == {"items": []}
    payload, key = calls[0]
    assert key == "SECRET"
    assert payload["model"] == "deepseek-flash"
    assert payload["response_format"] == {"type": "json_object"}
    assert payload["thinking"] == {"type": "disabled"}
    assert [m["role"] for m in payload["messages"]] == ["system", "user"]
    assert "SECRET" not in json.dumps(payload)

    c2 = deepseek.Client(tmp_path, post=post, key_provider=lambda: pytest.fail("key read on cache hit"))
    assert c2.complete_json("sys json", "user") == {"items": []}
    assert len(calls) == 1 and c2.cache_hits == 1 and c2.requests == 0
    assert all("SECRET" not in p.read_text() for p in tmp_path.iterdir())
    assert c.usage["total_tokens"] == 15


def test_retries_bad_json_then_gives_up_without_caching(tmp_path):
    replies = [{"choices": [{"message": {"content": "not json"}, "finish_reason": "stop"}]},
               fake_reply({"ok": 1}, finish="length")]
    c = deepseek.Client(tmp_path, post=lambda p, k: replies.pop(0), key_provider=lambda: "k")
    with pytest.raises(deepseek.DeepSeekError):
        c.complete_json("s", "u")
    assert list(tmp_path.glob("*.json")) == []


def test_truncated_response_is_reported_as_truncated_not_as_bad_json(tmp_path):
    cut = {"choices": [{"message": {"content": '{"items": [{"na'}, "finish_reason": "length"}]}
    c = deepseek.Client(tmp_path, post=lambda p, k: cut, key_provider=lambda: "k")
    with pytest.raises(deepseek.DeepSeekError, match="truncated"):
        c.complete_json("s", "u")


def _http(code):
    return deepseek.DeepSeekError(f"HTTP {code}", retryable=code == 429 or code >= 500)


def test_retries_once_with_backoff_on_rate_limit_and_server_errors(tmp_path):
    outcomes = [_http(429), fake_reply({"ok": 1})]
    sleeps = []

    def post(payload, key):
        out = outcomes.pop(0)
        if isinstance(out, Exception):
            raise out
        return out

    c = deepseek.Client(tmp_path, post=post, key_provider=lambda: "k", sleep=sleeps.append)
    assert c.complete_json("s", "u") == {"ok": 1}
    assert sleeps == [deepseek.RETRY_BACKOFF_S] and c.requests == 2

    outcomes[:] = [_http(503), _http(503)]
    c2 = deepseek.Client(tmp_path / "b", post=post, key_provider=lambda: "k", sleep=sleeps.append)
    with pytest.raises(deepseek.DeepSeekError):
        c2.complete_json("s", "u")
    assert c2.requests == 2


def test_client_errors_are_not_retried(tmp_path):
    calls = []

    def post(payload, key):
        calls.append(1)
        raise _http(401)

    c = deepseek.Client(tmp_path, post=post, key_provider=lambda: "k", sleep=lambda s: pytest.fail("slept"))
    with pytest.raises(deepseek.DeepSeekError):
        c.complete_json("s", "u")
    assert len(calls) == 1


def test_http_status_classification(monkeypatch):
    import urllib.error

    def boom(code):
        def urlopen(req, timeout):
            raise urllib.error.HTTPError("u", code, "m", {}, None)

        return urlopen

    for code, retryable in ((429, True), (500, True), (401, False), (400, False)):
        monkeypatch.setattr(deepseek.urllib.request, "urlopen", boom(code))
        with pytest.raises(deepseek.DeepSeekError) as exc:
            deepseek._post({}, "k")
        assert exc.value.retryable is retryable

    def timeout(req, timeout):
        raise TimeoutError()

    monkeypatch.setattr(deepseek.urllib.request, "urlopen", timeout)
    with pytest.raises(deepseek.DeepSeekError) as exc:
        deepseek._post({}, "k")
    assert exc.value.retryable


def test_corrupt_cache_file_counts_as_a_miss(tmp_path):
    c = deepseek.Client(tmp_path, post=lambda p, k: fake_reply({"fresh": 1}), key_provider=lambda: "k")
    payload = c.build_payload("s", "u")
    (tmp_path / f"{c.cache_key(payload)}.json").write_text("{not json")
    assert c.complete_json("s", "u") == {"fresh": 1}
    assert c.cache_hits == 0 and c.requests == 1
    assert c.complete_json("s", "u") == {"fresh": 1} and c.cache_hits == 1


def test_key_read_from_keychain_without_leaking_to_argv(monkeypatch):
    seen = {}

    def run(cmd, **kw):
        seen["cmd"] = cmd

        class R:
            stdout = "abc\n"

        return R()

    monkeypatch.setattr(deepseek.subprocess, "run", run)
    assert deepseek.read_api_key() == "abc"
    assert seen["cmd"][:2] == ["security", "find-generic-password"] and "abc" not in seen["cmd"]


# --- draft merging (mocked AI) -------------------------------------------------


class ScriptedClient:
    def __init__(self, replies):
        self.replies = replies
        self.prompts = []

    def complete_json(self, system, user, max_tokens=8000):
        self.prompts.append(user)
        return self.replies(len(self.prompts) - 1, user)


def test_vocab_merge_drops_conflicts_and_resolves_parents():
    v = Vocab()
    assert v.add({"name": "西红柿", "aliases": ["番茄", "番茄", "西红柿"]}, "vegetable")
    assert not v.add({"name": "番茄"}, "vegetable")  # existing alias
    assert v.add({"name": "圣女果", "aliases": ["番茄", "小番茄"], "parent": "番茄"}, "vegetable")
    assert v.items["圣女果"]["aliases"] == ["小番茄"]
    items = v.finish()
    assert next(i for i in items if i["name"] == "圣女果")["parent"] == "西红柿"


def test_draft_vocabulary_one_request_per_topic_and_includes_known_names():
    def replies(i, user):
        return {"items": [{"name": f"项{i}", "aliases": [f"别{i}"]}]}

    client = ScriptedClient(replies)
    v = Vocab()
    draft_vocabulary(client, v)
    assert len(client.prompts) == len(TOPICS) and len(v.items) == len(TOPICS)
    assert "项0" in client.prompts[1] and "品类：vegetable" in client.prompts[0]


def test_draft_map_batches_and_maps_aliases_to_standard_names():
    recipes = {"recipes": [{"ingredients": [
        {"name": "番茄 2 个", "kind": "ingredient"}, {"name": "葱姜蒜", "kind": "unknown"},
        {"name": "锅", "kind": "tool"}, {"name": "烧烤料", "kind": "ingredient"}]}]}
    v = Vocab()
    for n, a in (("西红柿", ["番茄"]), ("葱", []), ("姜", []), ("蒜", [])):
        v.add({"name": n, "aliases": a}, "vegetable")

    def replies(i, user):
        return {
            "mappings": {"番茄 2 个": ["番茄"], "葱姜蒜": ["葱", "姜", "蒜"], "烧烤料": ["烧烤料", "不存在"]},
            "new_items": [{"name": "烧烤料", "category": "seasoning"}],
        }

    m = draft_map(ScriptedClient(replies), v, recipes)
    assert m == {"番茄 2 个": ["西红柿"], "葱姜蒜": ["葱", "姜", "蒜"], "烧烤料": ["烧烤料"]}
    assert "锅" not in m
    assert any("unknown target" in line for line in v.log)
