"""Single-pass, paid synthetic recognition and audited recording collection.

`run` writes only to /tmp and never retries a sample: an .attempted marker is written before
launching the CLI. `collect` validates the format and checks for leaked identifiers before
copying synthetic-only responses to test-data/recordings/synthetic/. No key is handled here.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

from .evaluate import _parse_content
from .model import RECORDINGS_DIR, REPO_ROOT, VOCABULARY_PATH, Sample, sample_file, truth_file
from .scenarios import SAMPLES

TMP_ROOT = Path("/tmp/whilefresh-t7-synthetic")


def _request_kind(sample: Sample) -> str:
    return "order-text" if sample.kind == "text" else sample.kind


def _ensure_tmp(path: Path) -> None:
    target = path.resolve()
    if not target.is_relative_to(Path("/tmp")):
        raise ValueError(f"temporary recording directory must be under /tmp: {target}")
    path.mkdir(parents=True, exist_ok=True)


def _recording_path(tmp: Path, sample: Sample) -> Path:
    return tmp / (sample.id + ".json")


def _log_path(tmp: Path, sample: Sample) -> Path:
    return tmp / (sample.id + ".cli.log")


def _count_responses(tmp: Path) -> int:
    return sum(len(json.loads(p.read_text(encoding="utf-8")).get("responses", [])) for p in tmp.glob("*.json"))


def _audit(sample: Sample, doc: dict) -> None:
    """Only our own generated sample and the corresponding CLI format-1 response may enter git."""
    truth = json.loads(truth_file(sample).read_text(encoding="utf-8"))
    if truth["id"] != sample.id or not sample_file(sample).exists():
        raise ValueError(f"{sample.id}: generated source/truth missing")
    if doc.get("format") != 1 or not doc.get("responses") or not isinstance(doc["responses"], list):
        raise ValueError(f"{sample.id}: not a nonempty format-1 recording")
    req = doc.get("request", {})
    if req.get("inputKind") != _request_kind(sample) or req.get("promptVersion") != "v2":
        raise ValueError(f"{sample.id}: wrong input kind or prompt version")
    if req.get("imageCount") != (0 if sample.kind == "text" else 1):
        raise ValueError(f"{sample.id}: unexpected number of input images")
    for raw in doc["responses"]:
        choice = raw["choices"][0]
        if choice.get("finish_reason") == "length":
            raise ValueError(f"{sample.id}: response truncated")
        content = choice["message"]["content"]
        if not isinstance(content, str):
            raise ValueError(f"{sample.id}: no assistant content")
        _parse_content(content)
        if re.search(r"(?<!\d)1[3-9]\d{9}(?!\d)", content) or re.search(r"[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}", content):
            raise ValueError(f"{sample.id}: contains a phone/email-like identifier; audit manually")
    if len(json.dumps(doc, ensure_ascii=False)) > 2_000_000:
        raise ValueError(f"{sample.id}: unexpectedly large response; audit manually")


def _elapsed_ms(log: str) -> int | None:
    match = re.search(r"^source:\s+\S+\s+elapsed:\s+(\d+)\s+ms\b", log, re.MULTILINE)
    return int(match.group(1)) if match else None


def run(tmp: Path, cli: Path, limit: int = 30, samples: tuple[Sample, ...] = SAMPLES) -> int:
    _ensure_tmp(tmp)
    if not cli.is_file():
        raise ValueError(f"missing recognize-cli: {cli}")
    successful = 0
    for sample in samples:
        marker = tmp / (sample.id + ".attempted")
        out = _recording_path(tmp, sample)
        if marker.exists() or out.exists() or (RECORDINGS_DIR / out.name).exists():
            print(f"SKIP {sample.id}: already attempted/recorded")
            continue
        if _count_responses(tmp) >= limit:
            print(f"STOP before {sample.id}: reached {limit} HTTP responses")
            break
        truth = json.loads(truth_file(sample).read_text(encoding="utf-8"))
        source = sample_file(sample)
        if not source.is_file() or truth["id"] != sample.id:
            raise ValueError(f"{sample.id}: generate the sample first")
        cmd = [str(cli), "--vocab", str(VOCABULARY_PATH), "--record", str(out)]
        if sample.kind == "text":
            cmd += ["--text", str(source)]
        else:
            cmd += ["--kind", sample.kind, "--date", truth["capture_date"], str(source)]
        # Create the marker before calling paid service: if interrupted after a network call,
        # rerunning must never silently bill again for the same sample.
        marker.write_text("attempted\n", encoding="utf-8")
        print(f"RUN {sample.id} ({sample.kind})", flush=True)
        try:
            done = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, timeout=360)
            log = done.stdout + "\n" + done.stderr
            _log_path(tmp, sample).write_text(log, encoding="utf-8")
            if done.returncode:
                print(f"FAILED {sample.id}: CLI exited {done.returncode}; see {_log_path(tmp, sample)}")
                continue
            if not out.is_file():
                print(f"FAILED {sample.id}: CLI returned success without recording")
                continue
            doc = json.loads(out.read_text(encoding="utf-8"))
            _audit(sample, doc)
            elapsed = _elapsed_ms(log)
            tokens = sum(r.get("usage", {}).get("total_tokens", 0) for r in doc["responses"])
            print(f"OK {sample.id}: HTTP responses={len(doc['responses'])}, tokens={tokens}, "
                  f"elapsed={elapsed if elapsed is not None else 'missing'} ms; total responses={_count_responses(tmp)}")
            successful += 1
        except (subprocess.TimeoutExpired, ValueError, KeyError, IndexError, json.JSONDecodeError) as exc:
            print(f"FAILED {sample.id}: {exc}")
            # Nothing is reattempted automatically, even when the recording is incomplete.
    print(f"Finished: {successful} new successful samples; {_count_responses(tmp)} HTTP responses recorded in {tmp}")
    return successful


def collect(tmp: Path, samples: tuple[Sample, ...] = SAMPLES) -> tuple[int, list[str]]:
    """Called only after manual review of the tmp responses; add elapsed_ms to the repo copy."""
    _ensure_tmp(tmp)
    copied = 0
    failed = []
    for sample in samples:
        source = _recording_path(tmp, sample)
        dest = RECORDINGS_DIR / source.name
        if dest.exists():
            continue
        if not source.exists():
            failed.append(sample.id)
            continue
        try:
            doc = json.loads(source.read_text(encoding="utf-8"))
            _audit(sample, doc)
            log = _log_path(tmp, sample).read_text(encoding="utf-8")
            elapsed = _elapsed_ms(log)
            if elapsed is None:
                raise ValueError("missing CLI elapsed summary")
            doc["elapsed_ms"] = elapsed  # additive format-1 metadata; ReplayRecognizer ignores it
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            copied += 1
        except (OSError, ValueError, KeyError, IndexError) as exc:
            failed.append(f"{sample.id} ({exc})")
    return copied, failed


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Paid one-shot recognition, /tmp audit, then synthetic-only copy")
    parser.add_argument("action", choices=("run", "collect", "status"))
    parser.add_argument("--tmp", type=Path, default=TMP_ROOT)
    parser.add_argument("--cli", type=Path, default=REPO_ROOT / "build" / "desktop" / "tools" / "recognize-cli")
    parser.add_argument("--limit", type=int, default=30, help="Maximum HTTP responses (default 30)")
    args = parser.parse_args(argv)
    if args.action == "run":
        return 0 if run(args.tmp, args.cli, args.limit) else 1
    if args.action == "collect":
        copied, failed = collect(args.tmp)
        print(f"Copied {copied} synthetic recordings to {RECORDINGS_DIR}; missing/invalid: {', '.join(failed) if failed else 'none'}")
        return 1 if failed else 0
    _ensure_tmp(args.tmp)
    print(f"HTTP responses: {_count_responses(args.tmp)}; attempted: {len(list(args.tmp.glob('*.attempted')))}; "
          f"collected: {len(list(RECORDINGS_DIR.glob('*.json'))) if RECORDINGS_DIR.exists() else 0}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
