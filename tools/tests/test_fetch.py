import io
import socket
import tarfile
import unicodedata
import urllib.error
from pathlib import Path

import pytest

from whilefresh_tools.recipes import fetch
from whilefresh_tools.recipes.build import main


def _tar(entries) -> io.BytesIO:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tf:
        for name, kind, payload in entries:
            info = tarfile.TarInfo(name)
            if kind == "file":
                data = payload.encode()
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            else:
                info.type = tarfile.SYMTYPE if kind == "symlink" else tarfile.LNKTYPE
                info.linkname = payload
                tf.addfile(info)
    buf.seek(0)
    return buf


def test_extract_only_safe_markdown_with_nfc_names(tmp_path):
    nfd = unicodedata.normalize("NFD", "Café")
    assert nfd != "Café"
    archive = _tar(
        [
            ("repo-abc/dishes/meat_dish/ok.md", "file", "# ok"),
            ("repo-abc/dishes/staple/sub/" + nfd + ".md", "file", "# nfd"),
            ("../x/dishes/meat_dish/up.md", "file", "bad"),
            ("/abs/dishes/meat_dish/abs.md", "file", "bad"),
            ("repo-abc/dishes/../../escape.md", "file", "bad"),
            ("repo-abc/dishes/meat_dish/./dot.md", "file", "bad"),
            ("repo-abc/dishes/meat_dish/back\\slash.md", "file", "bad"),
            ("repo-abc/dishes/meat_dish/link.md", "symlink", "/etc/passwd"),
            ("repo-abc/dishes/meat_dish/hard.md", "hardlink", "repo-abc/dishes/meat_dish/ok.md"),
            ("repo-abc/docs/readme.md", "file", "bad"),
            ("repo-abc/dishes/meat_dish/pic.png", "file", "bad"),
            ("repo-abc/dishes/top.md", "file", "bad"),
        ]
    )
    dest = tmp_path / "cache"
    fetch._extract_dishes(archive, dest)
    found = sorted(p.relative_to(dest).as_posix() for p in dest.rglob("*") if p.is_file())
    assert found == ["dishes/meat_dish/ok.md", "dishes/staple/sub/Café.md"]
    assert unicodedata.is_normalized("NFC", found[1])
    assert [p.name for p in tmp_path.iterdir()] == ["cache"]


def test_oversized_member_skipped(tmp_path, monkeypatch):
    monkeypatch.setattr(fetch, "MAX_MEMBER_BYTES", 4)
    fetch._extract_dishes(
        _tar([("r/dishes/a/small.md", "file", "ok"), ("r/dishes/a/big.md", "file", "12345")]), tmp_path
    )
    assert [p.name for p in tmp_path.rglob("*.md")] == ["small.md"]


def _fail_open(exc):
    def opener(url, timeout=None):
        raise exc

    return opener


@pytest.mark.parametrize(
    "exc", [urllib.error.URLError("no route"), socket.timeout("timed out"), ConnectionResetError("reset")]
)
def test_network_errors_become_readable_one_liner(tmp_path, monkeypatch, exc):
    monkeypatch.setattr(fetch.urllib.request, "urlopen", _fail_open(exc))
    with pytest.raises(fetch.FetchError) as info:
        fetch.ensure_snapshot(tmp_path / "c")
    msg = str(info.value)
    assert "\n" not in msg and "WHILEFRESH_CACHE" in msg and "snapshot.tar.gz" in msg


class _Resp(io.BytesIO):
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def test_download_size_cap(tmp_path, monkeypatch):
    monkeypatch.setattr(fetch, "MAX_ARCHIVE_BYTES", 100)
    monkeypatch.setattr(fetch.urllib.request, "urlopen", lambda url, timeout=None: _Resp(b"x" * 500))
    with pytest.raises(fetch.FetchError, match="limit"):
        fetch.ensure_snapshot(tmp_path / "c")


def test_download_total_deadline(tmp_path, monkeypatch):
    clock = iter(range(0, 10_000, 200))
    monkeypatch.setattr(fetch.time, "monotonic", lambda: next(clock))
    monkeypatch.setattr(fetch.urllib.request, "urlopen", lambda url, timeout=None: _Resp(b"x" * (3 << 20)))
    with pytest.raises(fetch.FetchError, match="not finished"):
        fetch.ensure_snapshot(tmp_path / "c")


def test_manual_archive_records_sha256_and_marks_complete(tmp_path, monkeypatch):
    monkeypatch.setattr(fetch.urllib.request, "urlopen", _fail_open(AssertionError("no network")))
    cache = tmp_path / "c"
    cache.mkdir()
    data = _tar([("r/dishes/soup/a.md", "file", "# a")]).getvalue()
    (cache / fetch.MANUAL_ARCHIVE).write_bytes(data)
    assert fetch.ensure_snapshot(cache) == cache
    assert (cache / "dishes/soup/a.md").read_text() == "# a"
    import hashlib

    assert (cache / ".sha256").read_text().strip() == hashlib.sha256(data).hexdigest()
    assert (cache / ".complete").exists()
    # A completed cache never touches the network again.
    assert fetch.ensure_snapshot(cache) == cache


def test_corrupt_manual_archive_is_readable_error(tmp_path):
    cache = tmp_path / "c"
    cache.mkdir()
    (cache / fetch.MANUAL_ARCHIVE).write_bytes(b"not a tarball")
    with pytest.raises(fetch.FetchError, match="unreadable"):
        fetch.ensure_snapshot(cache)
    assert not (cache / ".complete").exists()


def test_cli_reports_fetch_error_without_traceback(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(fetch.urllib.request, "urlopen", _fail_open(urllib.error.URLError("down")))
    rc = main(["build", "--cache-dir", str(tmp_path / "c"), "--out", str(tmp_path / "o.json"), "--report", str(tmp_path / "r.md")])
    err = capsys.readouterr().err
    assert rc == 1 and err.startswith("error: ") and err.count("\n") == 1
    assert not (tmp_path / "o.json").exists()
