"""Minimal DeepSeek chat client for the one-off vocabulary draft.

The API key is read from the macOS keychain inside this process and is never
printed, logged, cached or passed on a command line. Responses are cached on
disk under the request hash so reruns cost nothing.
"""

import hashlib
import json
import os
import subprocess
import urllib.error
import urllib.request
from pathlib import Path

API_URL = "https://api.deepseek.com/chat/completions"
MODEL = "deepseek-flash"
KEYCHAIN_SERVICE = "landing-project-deepseek"
TIMEOUT_S = 600
MAX_ATTEMPTS = 2


class DeepSeekError(RuntimeError):
    pass


def default_cache_dir() -> Path:
    base = os.environ.get("WHILEFRESH_CACHE") or Path.home() / ".cache" / "whilefresh"
    return Path(base) / "vocabulary"


def read_api_key() -> str:
    try:
        out = subprocess.run(
            ["security", "find-generic-password", "-s", KEYCHAIN_SERVICE, "-w"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise DeepSeekError("cannot read the DeepSeek key from the keychain") from exc
    key = out.stdout.strip()
    if not key:
        raise DeepSeekError("the keychain entry for the DeepSeek key is empty")
    return key


def _post(payload: dict, key: str) -> dict:
    req = urllib.request.Request(
        API_URL,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json", "Authorization": f"Bearer {key}"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp:
            return json.load(resp)
    except urllib.error.HTTPError as exc:
        raise DeepSeekError(f"DeepSeek returned HTTP {exc.code}") from None
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise DeepSeekError(f"DeepSeek request failed: {type(exc).__name__}") from None


class Client:
    """``post`` and ``key_provider`` are injectable so tests never touch the network."""

    def __init__(self, cache_dir: Path | None = None, post=_post, key_provider=read_api_key):
        self.cache_dir = cache_dir or default_cache_dir()
        self._post = post
        self._key_provider = key_provider
        self._key: str | None = None
        self.requests = 0
        self.cache_hits = 0
        self.usage = {"prompt_tokens": 0, "completion_tokens": 0, "total_tokens": 0}

    @staticmethod
    def build_payload(system: str, user: str, max_tokens: int = 8000) -> dict:
        return {
            "model": MODEL,
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
            "response_format": {"type": "json_object"},
            "thinking": {"type": "disabled"},  # thinking tokens ate the whole budget and left no answer
            "max_tokens": max_tokens,
        }

    @staticmethod
    def cache_key(payload: dict) -> str:
        blob = json.dumps(payload, ensure_ascii=False, sort_keys=True).encode("utf-8")
        return hashlib.sha256(blob).hexdigest()

    def _cache_path(self, payload: dict) -> Path:
        return self.cache_dir / f"{self.cache_key(payload)}.json"

    def complete_json(self, system: str, user: str, max_tokens: int = 8000) -> dict:
        """Return the parsed JSON object the model answered with (cached)."""
        payload = self.build_payload(system, user, max_tokens)
        path = self._cache_path(payload)
        if path.exists():
            self.cache_hits += 1
            return json.loads(path.read_text(encoding="utf-8"))["parsed"]

        last = "no attempt made"
        for _ in range(MAX_ATTEMPTS):
            if self._key is None:
                self._key = self._key_provider()
            self.requests += 1
            data = self._post(payload, self._key)
            for k in self.usage:
                self.usage[k] += int((data.get("usage") or {}).get(k) or 0)
            try:
                choice = data["choices"][0]
                parsed = json.loads(choice["message"]["content"])
                if choice.get("finish_reason") == "length":
                    last = "response truncated (finish_reason=length)"
                    continue
                if not isinstance(parsed, dict):
                    last = "response is not a JSON object"
                    continue
            except (KeyError, IndexError, TypeError, json.JSONDecodeError):
                last = "response is not valid JSON"
                continue
            self.cache_dir.mkdir(parents=True, exist_ok=True)
            record = {"request": payload, "usage": data.get("usage"), "parsed": parsed}
            tmp = path.with_suffix(".tmp")
            tmp.write_text(json.dumps(record, ensure_ascii=False), encoding="utf-8")
            tmp.replace(path)
            return parsed
        raise DeepSeekError(f"giving up after {MAX_ATTEMPTS} attempts: {last}")
