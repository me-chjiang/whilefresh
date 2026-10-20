"""Offline evaluation of recognize-cli recordings against synthetic ground truth.

Pairing is by visible product name (plus specification as a tie breaker), independent of the
model's canonical label, so a wrong canonical name is still counted in the diagnostics. The
strict product TP requires both canonical name and cart selection to match. Optional occluded
receipt lines are excluded from denominators and may be recognised without penalty.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from difflib import SequenceMatcher
from pathlib import Path
from typing import Any

from .model import CATEGORIES, RECORDINGS_DIR, SYNTHETIC_DIR, UNCATEGORIZED, VOCABULARY_PATH, validate_truth


DATES = ("production_date_text", "expiry_date_text", "purchase_date_text")


@dataclass
class Stats:
    expected: int = 0
    predicted: int = 0
    matched: int = 0
    true_positive: int = 0
    canonical_correct: int = 0
    category_correct: int = 0
    quantity_correct: int = 0
    count_correct: int = 0
    gift_expected: int = 0
    gift_found: int = 0
    unchecked_expected: int = 0
    unchecked_found: int = 0
    visible_dates: int = 0
    correct_dates: int = 0
    invented_dates: int = 0
    noise_false_positives: int = 0
    other_false_positives: int = 0
    uncategorized: int = 0
    uncategorized_actionable: int = 0
    prompt_tokens: int = 0
    completion_tokens: int = 0
    total_tokens: int = 0
    elapsed_ms: int = 0
    elapsed_samples: int = 0
    api_responses: int = 0
    samples: int = 0
    source_correct: int = 0

    def __iadd__(self, other: "Stats") -> "Stats":
        for key in self.__dataclass_fields__:
            setattr(self, key, getattr(self, key) + getattr(other, key))
        return self

    @property
    def recall(self) -> float | None:
        return self.true_positive / self.expected if self.expected else None

    @property
    def precision(self) -> float | None:
        return self.true_positive / self.predicted if self.predicted else None

    @property
    def canonical_accuracy(self) -> float | None:
        return self.canonical_correct / self.matched if self.matched else None

    @property
    def category_accuracy(self) -> float | None:
        return self.category_correct / self.matched if self.matched else None

    @property
    def date_accuracy(self) -> float | None:
        return self.correct_dates / self.visible_dates if self.visible_dates else None


@dataclass
class Evaluation:
    id: str
    stats: Stats
    errors: list[str] = field(default_factory=list)
    ignored_kinds: list[str] = field(default_factory=list)
    ignored_summary: str = ""
    source: str = ""
    expected_source: str = ""
    category_rows: list[tuple[str, str | None]] = field(default_factory=list)


def _norm(value: Any) -> str:
    text = str(value or "").lower()
    text = re.sub(r"【[^】]*】|\[[^]]*\]", "", text)
    text = re.sub(r"[\s\-·*×x()（）\[\]{}。,.，：:＋+!！、/]+", "", text)
    return text


def _similar(a: Any, b: Any) -> float:
    left, right = _norm(a), _norm(b)
    if not left or not right:
        return 0.0
    if left == right:
        return 1.0
    return SequenceMatcher(None, left, right).ratio()


def _optional(value: Any) -> str | None:
    if value is None:
        return None
    string = str(value).strip()
    return string if string and string.lower() not in ("null", "none", "无", "未知", "-") else None


def _int(value: Any, fallback: int) -> int:
    try:
        parsed = int(value)
        return min(999, max(1, parsed))
    except (TypeError, ValueError):
        return fallback


def _bool(value: Any, fallback: bool) -> bool:
    if isinstance(value, bool):
        return value
    if str(value).lower() in ("true", "yes", "1", "是"):
        return True
    if str(value).lower() in ("false", "no", "0", "否"):
        return False
    return fallback


def _canonical(value: Any, vocabulary: dict[str, str]) -> str:
    return vocabulary.get(value, UNCATEGORIZED)


def load_vocabulary(path: Path = VOCABULARY_PATH) -> dict[str, str]:
    doc = json.loads(path.read_text(encoding="utf-8"))
    aliases = {UNCATEGORIZED: UNCATEGORIZED}
    for item in doc["items"]:
        aliases[item["name"]] = item["name"]
        aliases.update({alias: item["name"] for alias in item["aliases"]})
    return aliases


def _parse_content(content: str) -> dict:
    content = content.strip()
    if content.startswith("```"):
        content = content[content.find("\n") + 1:]
        content = content[:content.rfind("```")].strip() if "```" in content else content
    try:
        doc = json.loads(content)
    except json.JSONDecodeError:
        start, stop = content.find("{"), content.rfind("}")
        if start < 0 or stop < start:
            raise ValueError("response contains no JSON object") from None
        doc = json.loads(content[start:stop + 1])
    if isinstance(doc, list):
        doc = {"items": doc}
    if not isinstance(doc, dict) or not isinstance(doc.get("items"), list):
        raise ValueError("response JSON has no items array")
    return doc


def parse_recording(recording: dict, vocabulary: dict[str, str]) -> tuple[list[dict], str, str, Stats]:
    if recording.get("format") != 1 or not isinstance(recording.get("responses"), list):
        raise ValueError("not a format-1 recording")
    stats = Stats(api_responses=len(recording["responses"]))
    elapsed = recording.get("elapsed_ms")  # added by collect; CLI format-1 does not contain timing
    if isinstance(elapsed, (int, float)) and elapsed >= 0:
        stats.elapsed_ms = int(elapsed)
        stats.elapsed_samples = 1
    parts: list[list[dict]] = []
    source = ""
    ignored = ""
    for raw in recording["responses"]:
        choice = raw["choices"][0]
        doc = _parse_content(choice["message"]["content"])
        source = source or doc.get("source_type", "")
        ignored = ignored or doc.get("ignored_summary", "")
        part = []
        for item in doc["items"]:
            if not isinstance(item, dict):
                continue
            obj = dict(item)
            obj["raw_name"] = _optional(obj.get("raw_name")) or _optional(obj.get("canonical_name")) or ""
            if not obj["raw_name"]:
                continue
            obj["canonical_name"] = _canonical(_optional(obj.get("canonical_name")), vocabulary)
            obj["selected"] = _bool(obj.get("selected"), True)
            obj["is_gift"] = _bool(obj.get("is_gift"), False)
            obj["count"] = _int(obj.get("count"), 1)
            obj["quantity_text"] = _optional(obj.get("quantity_text"))
            for key in DATES:
                obj[key] = _optional(obj.get(key))
            obj["category"] = obj.get("category") if obj.get("category") in CATEGORIES else "food"
            try:
                image = int(obj.get("image_index"))
            except (TypeError, ValueError):
                image = -1
            obj["image_index"] = image - 1 if image > 0 else -1
            part.append(obj)
        parts.append(part)
        usage = raw.get("usage", {})
        stats.prompt_tokens += usage.get("prompt_tokens", 0)
        stats.completion_tokens += usage.get("completion_tokens", 0)
        stats.total_tokens += usage.get("total_tokens", 0)
    spans = recording.get("batches", [])
    items: list[dict] = []
    for index, part in enumerate(parts):
        shared_image = -1
        if index and len(spans) == len(parts):
            prev = spans[index - 1].get("lastImage", -1)
            if prev >= 0 and prev == spans[index].get("firstImage", -1):
                shared_image = prev
        if shared_image >= 0:
            previous_keys = {
                (i["raw_name"], i["quantity_text"], i["is_gift"])
                for i in parts[index - 1] if i["image_index"] in (-1, shared_image)
            }
            part = [
                i for i in part
                if i["image_index"] not in (-1, shared_image)
                or (i["raw_name"], i["quantity_text"], i["is_gift"]) not in previous_keys
            ]
        items.extend(part)
    return items, source, ignored, stats


def match_items(expected: list[dict], predicted: list[dict]) -> tuple[list[tuple[int, int]], list[int], list[int]]:
    """Max-similarity one-to-one pairing, independent of predicted canonical and selection.

    A raw name match is required: otherwise a recommended product with the same standard name
    could mask a missing purchased one. Same-name repetitions are paired by specification.
    """
    candidates: list[tuple[float, int, int]] = []
    for e, truth in enumerate(expected):
        for p, model in enumerate(predicted):
            similarity = _similar(truth["raw_name"], model["raw_name"])
            if similarity < 0.48:
                continue
            score = similarity
            if _norm(truth.get("quantity_text")) == _norm(model.get("quantity_text")):
                score += 0.16
            if truth.get("count") == model.get("count"):
                score += 0.03
            if truth.get("canonical_name") == model.get("canonical_name"):
                score += 0.01
            candidates.append((score, e, p))
    candidates.sort(key=lambda pair: (-pair[0], pair[1], pair[2]))
    paired: list[tuple[int, int]] = []
    used_e: set[int] = set()
    used_p: set[int] = set()
    for _, e, p in candidates:
        if e in used_e or p in used_p:
            continue
        paired.append((e, p))
        used_e.add(e)
        used_p.add(p)
    return sorted(paired), sorted(set(range(len(expected))) - used_e), sorted(set(range(len(predicted))) - used_p)


def _is_noise(item: dict, ignored_items: list[dict]) -> bool:
    name = item.get("raw_name", "")
    if any(_similar(name, noise["raw_name"]) >= 0.57 for noise in ignored_items):
        return True
    return any(word in name for word in ("配送费", "包装费", "满减", "优惠券", "换购", "热卖", "凑单", "广告"))


def evaluate_sample(truth: dict, recording: dict, vocabulary: dict[str, str]) -> Evaluation:
    errors = validate_truth(truth, set(vocabulary.values()))
    if errors:
        raise ValueError(f"{truth.get('id')}: {'; '.join(errors)}")
    predictions, source, ignored, usage = parse_recording(recording, vocabulary)
    stats = usage
    stats.samples = 1
    stats.source_correct = int(source == truth["source_type"])
    expected = truth["items"]
    pairs, unmatched_e, unmatched_p = match_items(expected, predictions)
    # The optional, occluded items are visible in the answer for documentation but not a
    # scoring obligation. If read, they are excluded from precision too.
    optional_matched = sum(1 for e, _ in pairs if expected[e]["optional"])
    stats.expected = sum(not item["optional"] for item in expected)
    stats.predicted = len(predictions) - optional_matched
    stats.uncategorized = sum(p["canonical_name"] == UNCATEGORIZED for p in predictions)
    stats.uncategorized_actionable = sum(p["canonical_name"] == UNCATEGORIZED and p["category"] != "irrelevant"
                                         for p in predictions)
    stats.gift_expected = sum(p["is_gift"] for p in expected if not p["optional"])
    stats.unchecked_expected = sum(not p["selected"] for p in expected if not p["optional"])
    details: list[str] = []
    category_rows: list[tuple[str, str | None]] = []
    for e, p in pairs:
        want, got = expected[e], predictions[p]
        if want["optional"]:
            continue
        category_rows.append((want["category"], got["category"]))
        stats.matched += 1
        canonical_ok = got["canonical_name"] == want["canonical_name"]
        selected_ok = got["selected"] == want["selected"]
        if canonical_ok and selected_ok:
            stats.true_positive += 1
        stats.canonical_correct += canonical_ok
        stats.category_correct += got["category"] == want["category"]
        stats.quantity_correct += _norm(got["quantity_text"]) == _norm(want["quantity_text"])
        stats.count_correct += got["count"] == want["count"]
        stats.gift_found += bool(want["is_gift"] and got["is_gift"])
        stats.unchecked_found += bool(not want["selected"] and not got["selected"])
        if not (canonical_ok and selected_ok and got["category"] == want["category"]):
            details.append(
                f"{want['raw_name']} → {got['canonical_name']} / {got['category']} / "
                f"{'勾选' if got['selected'] else '未勾选'}"
                f"（应为 {want['canonical_name']} / {want['category']} / "
                f"{'勾选' if want['selected'] else '未勾选'}）"
            )
        if got["is_gift"] != want["is_gift"]:
            details.append(f"赠品标记错误：{want['raw_name']}")
        if got["count"] != want["count"]:
            details.append(f"件数：{want['raw_name']} 应 {want['count']}，得 {got['count']}")
        for key in DATES:
            actual, ideal = got[key], want[key]
            if ideal is not None:
                stats.visible_dates += 1
                if actual == ideal:
                    stats.correct_dates += 1
                else:
                    details.append(f"日期：{want['raw_name']} {key} 应 {ideal!r}，得 {actual!r}")
            elif actual is not None:
                stats.invented_dates += 1
    for e in unmatched_e:
        want = expected[e]
        if not want["optional"]:
            category_rows.append((want["category"], None))
            for key in DATES:
                if want[key] is not None:
                    stats.visible_dates += 1
            details.append(f"漏识：{want['raw_name']}（{want['canonical_name']}）")
    for p in unmatched_p:
        got = predictions[p]
        if _is_noise(got, truth["ignored_items"]):
            stats.noise_false_positives += 1
            details.append(f"误纳入无关内容：{got['raw_name']}")
        else:
            stats.other_false_positives += 1
            details.append(f"多余商品：{got['raw_name']}")
    return Evaluation(truth["id"], stats, details, truth["ignored_kinds"], ignored,
                      source, truth["source_type"], category_rows)


def _percent(value: float | None) -> str:
    return f"{value:.1%}" if value is not None else "—"


def _escape_md(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render_report(results: list[Evaluation], missing: list[str], errors: dict[str, str]) -> str:
    total = Stats()
    for result in results:
        total += result.stats
    lines = [
        "# 合成样本识别基线",
        "",
        "- 来源：本仓库生成的虚构样本；逐样本真实 DeepSeek `deepseek-flash` 录制，不含私人真实样例。",
        "- 首版只记录基线，不设达标阈值；真实样例验收需另在 `private/` 完成。",
        f"- 覆盖：{len(results)} 个已录制样本；缺失 {len(missing)} 个；读取失败 {len(errors)} 个；底层 HTTP 响应 {total.api_responses} 个。",
        "- 商品以**标准名 + 勾选状态**同时正确为 TP；每个商品行计 1，不按购买件数展开。",
        "- 标准名、分类、规格和件数正确率在**可配对商品行**上计算；配对依据原商品名（允许少量错字），不依赖标准名。",
        "- 小票遮挡商品标记 `optional: true`，不计入召回/精确/日期分母；无关商品（如已买纸巾）计入商品行。推荐/费用等不是商品。",
        "- 日期只考察标准答案**可见的原文**，按字段逐个精确比较；不按拍摄日期换算相对日期。",
        "- `elapsed_ms` 是采集时从 CLI 摘要写入录制副本的补充字段（原生 format-1 没有时长）；缺失则不计平均耗时。",
        "",
        "| 样本 | 来源类型 | 召回 | 精确 | 标准名 | 分类 | 赠品 | 未勾选 | 日期 | 无关误纳入 | 未归类 | token | 耗时/s |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for result in results:
        s = result.stats
        time = f"{s.elapsed_ms / 1000:.1f}" if s.elapsed_samples else "—"
        source = result.source if s.source_correct else f"{result.expected_source}→{result.source}"
        lines.append(
            f"| {result.id} | {source} | {_percent(s.recall)} ({s.true_positive}/{s.expected}) | "
            f"{_percent(s.precision)} ({s.true_positive}/{s.predicted}) | "
            f"{_percent(s.canonical_accuracy)} ({s.canonical_correct}/{s.matched}) | "
            f"{_percent(s.category_accuracy)} ({s.category_correct}/{s.matched}) | "
            f"{s.gift_found}/{s.gift_expected} | {s.unchecked_found}/{s.unchecked_expected} | "
            f"{s.correct_dates}/{s.visible_dates} | {s.noise_false_positives} | "
            f"{s.uncategorized} | {s.total_tokens} | {time} |"
        )
    avg = f"{total.elapsed_ms / total.elapsed_samples / 1000:.1f}" if total.elapsed_samples else "—"
    lines.extend(
        [
            f"| **总体** | **{total.source_correct}/{total.samples}** | **{_percent(total.recall)} ({total.true_positive}/{total.expected})** | "
            f"**{_percent(total.precision)} ({total.true_positive}/{total.predicted})** | "
            f"**{_percent(total.canonical_accuracy)} ({total.canonical_correct}/{total.matched})** | "
            f"**{_percent(total.category_accuracy)} ({total.category_correct}/{total.matched})** | "
            f"**{total.gift_found}/{total.gift_expected}** | **{total.unchecked_found}/{total.unchecked_expected}** | "
            f"**{total.correct_dates}/{total.visible_dates}** | **{total.noise_false_positives}** | "
            f"**{total.uncategorized}** | **{total.total_tokens}** | **{avg}** |",
            "",
            f"总体细项：规格 {_percent(total.quantity_correct / total.matched if total.matched else None)} "
            f"({total.quantity_correct}/{total.matched})；件数 "
            f"{_percent(total.count_correct / total.matched if total.matched else None)} "
            f"({total.count_correct}/{total.matched})；无关内容误纳入 {total.noise_false_positives}，"
            f"其他多余商品 {total.other_false_positives}；无依据日期 {total.invented_dates}；"
            f"未归类 {total.uncategorized} 中 {total.uncategorized_actionable} 件食材/调料仍需归名、"
            f"其余为已购日用品等无关商品。",
            f"Token：prompt {total.prompt_tokens}，completion {total.completion_tokens}，total {total.total_tokens}；"
            f"可用时长 {total.elapsed_samples}/{total.samples}，平均 {avg} 秒/样本。",
            "",
            "## 食材 / 调料 / 无关分类",
            "",
            "| 标准答案 | 样本商品行 | 正确归类 | 误归食材 | 误归调料 | 误归无关 | 漏识 |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for category in CATEGORIES:
        rows = [got for result in results for want, got in result.category_rows if want == category]
        lines.append(
            f"| {category} | {len(rows)} | {rows.count(category)} | {rows.count('food') - (rows.count(category) if category == 'food' else 0)} | "
            f"{rows.count('seasoning') - (rows.count(category) if category == 'seasoning' else 0)} | "
            f"{rows.count('irrelevant') - (rows.count(category) if category == 'irrelevant' else 0)} | {rows.count(None)} |"
        )
    lines.extend([
            "",
            "## 典型错误和待复核样本",
            "",
        ]
    )
    worst = sorted(results, key=lambda r: (r.stats.recall if r.stats.recall is not None else 1,
                                           r.stats.precision if r.stats.precision is not None else 1,
                                           -r.stats.expected))[:3]
    for result in worst:
        s = result.stats
        lines.append(f"### {result.id}（召回 {_percent(s.recall)}，精确 {_percent(s.precision)}）")
        lines.append("")
        lines.extend(f"- {_escape_md(err)}" for err in result.errors[:12])
        if len(result.errors) > 12:
            lines.append(f"- 另有 {len(result.errors) - 12} 处差异，见录制文件。")
        if not result.errors:
            lines.append(
                f"- 无商品配对/分类差异；仅 {sum(bool(r.errors) for r in results)} 份样本出现商品错误，"
                f"本样本 {s.expected} 行并列满分，按复杂度列入观察。"
            )
        lines.append("")
    lines.extend(["## 来源类型差异", ""])
    source_errors = [r for r in results if not r.stats.source_correct]
    if source_errors:
        lines.extend(f"- {r.id}：应为 `{r.expected_source}`，识别为 `{r.source}`。" for r in source_errors)
    else:
        lines.append("所有录制的来源类型均与标准答案一致。")
    lines.extend(["", "## 数据范围与未覆盖", ""])
    if missing:
        lines.append("缺失录制：" + "、".join(missing) + "。")
    if errors:
        lines.extend(f"- {id_}: {_escape_md(msg)}" for id_, msg in errors.items())
    if not missing and not errors:
        lines.append("合成样本均有录制；不含真实样例的个人数据或校对结果。")
    lines.append("")
    return "\n".join(lines)


def evaluate_all(truth_root: Path = SYNTHETIC_DIR, recordings_root: Path = RECORDINGS_DIR,
                 vocabulary_path: Path = VOCABULARY_PATH) -> tuple[list[Evaluation], list[str], dict[str, str]]:
    vocab = load_vocabulary(vocabulary_path)
    results: list[Evaluation] = []
    missing: list[str] = []
    errors: dict[str, str] = {}
    for path in sorted(truth_root.glob("*/*.json")):
        truth = json.loads(path.read_text(encoding="utf-8"))
        rec_path = recordings_root / (truth["id"] + ".json")
        if not rec_path.exists():
            missing.append(truth["id"])
            continue
        try:
            recording = json.loads(rec_path.read_text(encoding="utf-8"))
            expected_kind = {"screenshot": "screenshot", "receipt": "receipt", "text": "order-text"}[truth["sample_kind"]]
            if recording.get("request", {}).get("inputKind") != expected_kind:
                raise ValueError(f"request inputKind mismatches {expected_kind}")
            results.append(evaluate_sample(truth, recording, vocab))
        except (ValueError, KeyError, TypeError, IndexError) as exc:
            errors[truth["id"]] = str(exc)
    return results, missing, errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare synthetic recordings to ground-truth JSON (offline)")
    parser.add_argument("--truth", type=Path, default=SYNTHETIC_DIR)
    parser.add_argument("--recordings", type=Path, default=RECORDINGS_DIR)
    parser.add_argument("--vocab", type=Path, default=VOCABULARY_PATH)
    parser.add_argument("--out", type=Path, help="Write Markdown report; default stdout")
    args = parser.parse_args(argv)
    results, missing, errors = evaluate_all(args.truth, args.recordings, args.vocab)
    report = render_report(results, missing, errors)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(report, encoding="utf-8", newline="\n")
        print(f"evaluated {len(results)} samples; missing {len(missing)}, errors {len(errors)} -> {args.out}")
    else:
        print(report)
    return 1 if missing or errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
