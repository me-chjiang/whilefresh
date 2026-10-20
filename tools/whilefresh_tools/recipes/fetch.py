"""Download a pinned HowToCook snapshot into an ignored cache directory."""

import os
import tarfile
import tempfile
import unicodedata
import urllib.request
from pathlib import Path
from typing import IO

REPO = "Anduin2017/HowToCook"
COMMIT = "0e106eea348c734fe1586f781202994439cf98a5"
REPO_URL = f"https://github.com/{REPO}"

_DONE_MARK = ".complete"


def default_cache_dir(commit: str = COMMIT) -> Path:
    base = os.environ.get("WHILEFRESH_CACHE") or Path.home() / ".cache" / "whilefresh"
    return Path(base) / "howtocook" / commit


def ensure_snapshot(cache_dir: Path, commit: str = COMMIT) -> Path:
    """Return the directory holding ``dishes/``, downloading it if not cached."""
    if (cache_dir / _DONE_MARK).exists():
        return cache_dir
    url = f"https://codeload.github.com/{REPO}/tar.gz/{commit}"
    cache_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryFile() as tmp:
        with urllib.request.urlopen(url, timeout=120) as resp:
            while chunk := resp.read(1 << 20):
                tmp.write(chunk)
        tmp.seek(0)
        _extract_dishes(tmp, cache_dir)
    (cache_dir / _DONE_MARK).write_text(commit + "\n", encoding="utf-8")
    return cache_dir


def _extract_dishes(fileobj: IO[bytes], dest: Path) -> None:
    with tarfile.open(fileobj=fileobj, mode="r|gz") as tf:
        for member in tf:
            if not member.isfile():
                continue
            # Drop the "<repo>-<commit>/" prefix; keep only dishes/**/*.md.
            parts = member.name.split("/")[1:]
            if len(parts) < 3 or parts[0] != "dishes" or not parts[-1].endswith(".md"):
                continue
            if any(p in ("", ".", "..") for p in parts):
                continue
            target = dest.joinpath(*(unicodedata.normalize("NFC", p) for p in parts))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(tf.extractfile(member).read())
