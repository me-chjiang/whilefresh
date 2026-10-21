"""Offline generator for fictional shopping screenshots, receipt photos and pasted text."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from .chrome import render_page
from .model import VOCABULARY_PATH, Sample, build_truth, sample_dir, sample_file, truth_file, validate_truth
from .photos import photograph_receipt
from .receipts import render_receipt
from .scenarios import SAMPLES
from .screens import RENDERERS
from .texts import render_text


def _write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def render_assets(sample: Sample) -> tuple[str | None, str | None, int | None]:
    """Return (HTML, order text, CSS width); pure and deterministic."""
    if sample.kind == "screenshot":
        html, width = RENDERERS[sample.style](sample)
        return html, None, width
    if sample.kind == "receipt":
        html, width = render_receipt(sample)
        return html, None, width
    if sample.kind == "text":
        return None, render_text(sample), None
    raise ValueError(f"unknown sample kind: {sample.kind}")


def generate(samples: tuple[Sample, ...] = SAMPLES, *, render: bool = True) -> list[tuple[str, tuple[int, int] | None]]:
    vocab = json.loads(VOCABULARY_PATH.read_text(encoding="utf-8"))
    names = {item["name"] for item in vocab["items"]}
    results = []
    for sample in samples:
        truth = build_truth(sample)
        errors = validate_truth(truth, names)
        if errors:
            raise ValueError(f"{sample.id}: {'; '.join(errors)}")
        sample_dir(sample).mkdir(parents=True, exist_ok=True)
        html, text, width = render_assets(sample)
        _write(truth_file(sample), json.dumps(truth, ensure_ascii=False, indent=2) + "\n")
        if html is not None:
            _write(sample_dir(sample) / (sample.id + ".html"), html)
        if text is not None:
            _write(sample_file(sample), text)
        size = None
        if render and html is not None and width is not None:
            if sample.kind == "screenshot":
                size = render_page(sample_dir(sample) / (sample.id + ".html"), sample_file(sample), width)
            else:
                with tempfile.TemporaryDirectory(prefix="whilefresh-receipt-") as tmp:
                    png = Path(tmp) / "receipt.png"
                    render_page(sample_dir(sample) / (sample.id + ".html"), png, width)
                    size = photograph_receipt(png, sample_file(sample), sample)
        results.append((sample.id, size))
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate fictional, deterministic recognition samples")
    parser.add_argument("--no-render", action="store_true", help="Write HTML/text/truth only; skip Chrome and JPEGs")
    parser.add_argument("--id", action="append", dest="ids", help="Generate only this sample ID (repeatable)")
    args = parser.parse_args(argv)
    samples = tuple(s for s in SAMPLES if not args.ids or s.id in args.ids)
    unknown = set(args.ids or ()) - {s.id for s in samples}
    if unknown:
        parser.error(f"unknown IDs: {', '.join(sorted(unknown))}")
    for id_, size in generate(samples, render=not args.no_render):
        print(f"{id_}: {size if size else 'text/HTML/truth only'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
