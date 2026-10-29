"""Synthetic generator and offline recognition evaluator (no network or Chrome needed)."""

from __future__ import annotations

import hashlib
import json
from copy import deepcopy
from pathlib import Path

import pytest

from whilefresh_tools.synthetic import regress
from whilefresh_tools.synthetic.evaluate import (
    evaluate_sample,
    load_vocabulary,
    match_items,
    parse_recording,
    render_report,
)
from whilefresh_tools.synthetic.generate import render_assets
from whilefresh_tools.synthetic.model import build_truth, validate_truth
from whilefresh_tools.synthetic.scenarios import SAMPLES

FIXTURES = Path(__file__).parent / "fixtures"


def fixture(name: str) -> dict:
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


def test_paid_regression_counts_failed_attempts(tmp_path, monkeypatch):
    cli = tmp_path / "recognize-cli"
    cli.touch()
    sources = tmp_path / "sources"
    sources.mkdir()
    for sample in SAMPLES[:2]:
        (sources / (sample.id + ".txt")).write_text("sample", encoding="utf-8")
        (sources / (sample.id + ".json")).write_text(json.dumps({"id": sample.id, "capture_date": "2026-10-21"}), encoding="utf-8")
    monkeypatch.setattr(regress, "sample_file", lambda sample: sources / (sample.id + ".txt"))
    monkeypatch.setattr(regress, "truth_file", lambda sample: sources / (sample.id + ".json"))
    monkeypatch.setattr(regress, "RECORDINGS_DIR", tmp_path / "recordings")
    monkeypatch.setattr(regress, "_ensure_tmp", lambda path: path.mkdir(parents=True, exist_ok=True))
    calls = []

    def fail(cmd, **kwargs):
        calls.append(cmd)
        return regress.subprocess.CompletedProcess(cmd, 1, "", "failed")

    monkeypatch.setattr(regress.subprocess, "run", fail)
    assert regress.run(tmp_path, cli, limit=1, samples=SAMPLES[:2]) == 0
    assert len(calls) == 1
    assert regress._count_attempts(tmp_path) == 1


@pytest.fixture(scope="module")
def vocabulary() -> dict[str, str]:
    return load_vocabulary()


def test_catalogue_and_schema(vocabulary):
    assert len(SAMPLES) == 23
    assert len({s.id for s in SAMPLES}) == len(SAMPLES)
    assert [sum(s.kind == kind for s in SAMPLES) for kind in ("screenshot", "receipt", "text")] == [13, 6, 4]
    names = set(vocabulary.values())
    for sample in SAMPLES:
        truth = build_truth(sample)
        assert validate_truth(truth, names) == [], sample.id
        assert truth["items"]
        assert truth["file"].startswith(sample.id)
        assert sample.capture_date.isoformat() == truth["capture_date"]
        assert all(item["category"] in ("food", "seasoning", "irrelevant") for item in truth["items"])
        assert all(item["canonical_name"] in names for item in truth["items"])


def test_assets_and_ground_truth_are_repeatable():
    for sample in SAMPLES:
        html, text, width = render_assets(sample)
        answer = json.dumps(build_truth(sample), ensure_ascii=False, indent=2) + "\n"
        h2, t2, w2 = render_assets(sample)
        a2 = json.dumps(build_truth(sample), ensure_ascii=False, indent=2) + "\n"
        assert (html, text, width, answer) == (h2, t2, w2, a2)
        assert html or text
        assert len(answer) > 200
        if sample.kind == "text":
            assert text and html is None and width is None
        else:
            assert html and text is None and width in (320, 360, 375, 390)
            assert 'id="end"' in html


def test_fixed_seed_regression_hashes():
    # Catch accidental changes to the rendered HTML/gold records, not Chrome/JPEG pixels.
    sample = SAMPLES[0]
    html, _, _ = render_assets(sample)
    truth = json.dumps(build_truth(sample), ensure_ascii=False, indent=2) + "\n"
    assert hashlib.sha256(html.encode("utf-8")).hexdigest() == "ba505fc7076a2110b5f03f3c09a81976a0282691df14f3e0205ed2d5eef0ae8f"
    assert hashlib.sha256(truth.encode("utf-8")).hexdigest() == "71c2c87d920bed3876ffdb6dad73f73c30d4dd51753a8fe9b41119a902984db9"


def test_schema_rejects_bad_fields_and_unknown_canonical(vocabulary):
    truth = build_truth(SAMPLES[0])
    bad = deepcopy(truth)
    bad["items"][0]["selected"] = "true"
    bad["items"][0]["canonical_name"] = "不存在的菜"
    bad["capture_date"] = "2026-99-80"
    assert len(validate_truth(bad, set(vocabulary.values()))) >= 3


def test_hand_recording_counts_metrics_and_errors(vocabulary):
    gold = fixture("synthetic-gold.json")
    rec = fixture("synthetic-recording.json")
    result = evaluate_sample(gold, rec, vocabulary)
    stats = result.stats
    assert (stats.expected, stats.predicted, stats.matched, stats.true_positive) == (4, 5, 4, 3)
    assert (stats.canonical_correct, stats.category_correct) == (4, 3)
    assert (stats.gift_found, stats.gift_expected) == (0, 1)
    assert (stats.unchecked_found, stats.unchecked_expected) == (0, 1)
    assert (stats.correct_dates, stats.visible_dates) == (1, 2)
    assert (stats.noise_false_positives, stats.other_false_positives) == (1, 0)
    assert (stats.uncategorized, stats.total_tokens, stats.elapsed_ms) == (2, 900, 9200)
    assert stats.recall == pytest.approx(0.75)
    assert stats.precision == pytest.approx(0.6)
    assert stats.canonical_accuracy == 1
    assert stats.category_accuracy == 0.75
    assert result.source == "cart"
    assert any("赠品标记错误" in err for err in result.errors)
    assert any("日期" in err for err in result.errors)
    report = render_report([result], [], {})
    assert "75.0%" in report and "0/1" in report and "900" in report
    assert "食材 / 调料 / 无关分类" in report


def test_wrong_canonical_is_paired_for_diagnostics(vocabulary):
    gold = fixture("synthetic-gold.json")
    rec = fixture("synthetic-recording.json")
    text = rec["responses"][0]["choices"][0]["message"]["content"]
    doc = json.loads(text.removeprefix("```json\n").removesuffix("\n```"))
    doc["items"][0]["canonical_name"] = "西红柿"
    rec["responses"][0]["choices"][0]["message"]["content"] = json.dumps(doc, ensure_ascii=False)
    s = evaluate_sample(gold, rec, vocabulary).stats
    assert s.matched == 4 and s.canonical_correct == 3 and s.true_positive == 2
    assert s.category_correct == 3


def test_duplicate_items_are_distinct_but_batch_overlaps_drop(vocabulary):
    gold = fixture("synthetic-gold.json")
    rec = fixture("synthetic-recording.json")
    text = rec["responses"][0]["choices"][0]["message"]["content"]
    doc = json.loads(text.removeprefix("```json\n").removesuffix("\n```"))
    egg = deepcopy(doc["items"][0])
    doc["items"].insert(1, egg)  # same SKU bought as two separate product rows
    rec["responses"][0]["choices"][0]["message"]["content"] = json.dumps(doc, ensure_ascii=False)
    got, _, _, _ = parse_recording(rec, vocabulary)
    assert sum(i["raw_name"] == "鲜鸡蛋10枚" for i in got) == 2

    # A shared image across two API requests: the later duplicate comes from slice overlap.
    second = deepcopy(rec["responses"][0])
    second_doc = {"source_type": "cart", "items": [egg]}
    second["choices"][0]["message"]["content"] = json.dumps(second_doc, ensure_ascii=False)
    second["usage"] = {"prompt_tokens": 100, "completion_tokens": 10, "total_tokens": 110}
    rec["responses"].append(second)
    rec["batches"] = [{"firstImage": 0, "lastImage": 0}, {"firstImage": 0, "lastImage": 0}]
    got, _, _, stats = parse_recording(rec, vocabulary)
    assert sum(i["raw_name"] == "鲜鸡蛋10枚" for i in got) == 2
    assert stats.total_tokens == 1010 and stats.api_responses == 2


def test_same_canonical_without_raw_match_is_not_a_true_positive(vocabulary):
    gold = fixture("synthetic-gold.json")
    rec = fixture("synthetic-recording.json")
    text = rec["responses"][0]["choices"][0]["message"]["content"]
    doc = json.loads(text.removeprefix("```json\n").removesuffix("\n```"))
    doc["items"][0]["raw_name"] = "热卖金谷大米"
    doc["items"][0]["canonical_name"] = "鸡蛋"
    rec["responses"][0]["choices"][0]["message"]["content"] = json.dumps(doc, ensure_ascii=False)
    result = evaluate_sample(gold, rec, vocabulary)
    assert result.stats.true_positive == 2
    assert result.stats.noise_false_positives == 2
    assert any("漏识：鲜鸡蛋10枚" in err for err in result.errors)


def test_optional_occluded_line_excluded_from_denominators(vocabulary):
    gold = fixture("synthetic-gold.json")
    gold["items"][0]["optional"] = True
    rec = fixture("synthetic-recording.json")
    stats = evaluate_sample(gold, rec, vocabulary).stats
    assert stats.expected == 3 and stats.predicted == 4 and stats.true_positive == 2
    assert stats.visible_dates == 1


def test_one_to_one_pairing_prefers_matching_spec():
    expected = [
        {"raw_name": "鲜鸡蛋", "quantity_text": "10枚"},
        {"raw_name": "鲜鸡蛋", "quantity_text": "20枚"},
    ]
    got = [
        {"raw_name": "鲜鸡蛋", "quantity_text": "20枚"},
        {"raw_name": "鲜鸡蛋", "quantity_text": "10枚"},
    ]
    pairs, misses, extras = match_items(expected, got)
    assert pairs == [(0, 1), (1, 0)]
    assert not misses and not extras
