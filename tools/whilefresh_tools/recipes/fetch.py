"""Download a pinned HowToCook snapshot into an ignored cache directory."""

import hashlib
import os
import shutil
import socket
import tarfile
import tempfile
import time
import unicodedata
import urllib.error
import urllib.request
import zlib
from pathlib import Path
from typing import IO

REPO = "Anduin2017/HowToCook"
COMMIT = "0e106eea348c734fe1586f781202994439cf98a5"
REPO_URL = f"https://github.com/{REPO}"

_DONE_MARK = ".complete"
_SHA_FILE = ".sha256"
MANUAL_ARCHIVE = "snapshot.tar.gz"

DOWNLOAD_TIMEOUT_S = 300  # whole download, not a single socket read
MAX_ARCHIVE_BYTES = 400 * 1024 * 1024  # the pinned archive is ~110 MB (images included)
MAX_MEMBER_BYTES = 5 * 1024 * 1024  # a recipe is a few KB; this only guards against bombs


class FetchError(RuntimeError):
    """A download problem, worded for the person running the tool."""


def default_cache_dir(commit: str = COMMIT) -> Path:
    base = os.environ.get("WHILEFRESH_CACHE") or Path.home() / ".cache" / "whilefresh"
    return Path(base) / "howtocook" / commit


def ensure_snapshot(cache_dir: Path, commit: str = COMMIT) -> Path:
    """Return the directory holding ``dishes/``, downloading it if not cached.

    A hand-downloaded archive placed at ``<cache_dir>/snapshot.tar.gz`` is used
    instead of the network.
    """
    if (cache_dir / _DONE_MARK).exists():
        return cache_dir
    cache_dir.mkdir(parents=True, exist_ok=True)
    manual = cache_dir / MANUAL_ARCHIVE
    with tempfile.TemporaryFile() as tmp:
        if manual.is_file():
            with manual.open("rb") as src:
                shutil.copyfileobj(src, tmp)
        else:
            _download(f"https://codeload.github.com/{REPO}/tar.gz/{commit}", tmp, manual)
        sha256 = _sha256(tmp)
        tmp.seek(0)
        try:
            _extract_dishes(tmp, cache_dir)
        except (tarfile.TarError, EOFError, zlib.error, OSError) as exc:
            raise FetchError(f"snapshot archive is unreadable ({exc}); delete {manual} and retry") from None
    (cache_dir / _SHA_FILE).write_text(sha256 + "\n", encoding="utf-8")
    (cache_dir / _DONE_MARK).write_text(commit + "\n", encoding="utf-8")
    return cache_dir


def _download(url: str, out: IO[bytes], manual: Path) -> None:
    hint = (
        f"set WHILEFRESH_CACHE to another cache location, or download {url} "
        f"yourself and save it as {manual}"
    )
    deadline = time.monotonic() + DOWNLOAD_TIMEOUT_S
    total = 0
    try:
        with urllib.request.urlopen(url, timeout=30) as resp:
            while chunk := resp.read(1 << 20):
                total += len(chunk)
                if total > MAX_ARCHIVE_BYTES:
                    raise FetchError(f"download exceeds {MAX_ARCHIVE_BYTES >> 20} MB limit; {hint}")
                if time.monotonic() > deadline:
                    raise FetchError(f"download not finished within {DOWNLOAD_TIMEOUT_S}s; {hint}")
                out.write(chunk)
    except (urllib.error.URLError, socket.timeout, TimeoutError, ConnectionError) as exc:
        raise FetchError(f"cannot download HowToCook snapshot ({exc}); {hint}") from None


def _sha256(fileobj: IO[bytes]) -> str:
    fileobj.seek(0)
    digest = hashlib.sha256()
    while chunk := fileobj.read(1 << 20):
        digest.update(chunk)
    return digest.hexdigest()


def _extract_dishes(fileobj: IO[bytes], dest: Path) -> None:
    """Write only regular ``dishes/**/*.md`` files, with NFC names, under ``dest``."""
    with tarfile.open(fileobj=fileobj, mode="r|gz") as tf:
        for member in tf:
            if not member.isreg() or member.size > MAX_MEMBER_BYTES:
                continue  # also skips symlinks and hardlinks
            raw = member.name.split("/")
            # Drop the "<repo>-<commit>/" prefix; keep only dishes/**/*.md.
            if raw[0] in ("", ".", "..") or "\\" in member.name:
                continue
            parts = raw[1:]
            if len(parts) < 3 or parts[0] != "dishes" or not parts[-1].endswith(".md"):
                continue
            if any(p in ("", ".", "..") for p in parts):
                continue
            target = dest.joinpath(*(unicodedata.normalize("NFC", p) for p in parts))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(tf.extractfile(member).read())
