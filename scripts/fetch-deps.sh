#!/usr/bin/env bash
# Fetch third-party build dependencies that are too large to vendor into the repo.
set -euo pipefail

ANDROID_OPENSSL_URL="https://github.com/KDAB/android_openssl.git"
# 2026-01-26 "Update binaries with correct symver values (#76)"; ships OpenSSL 3.x for Qt 6.5+.
ANDROID_OPENSSL_COMMIT="b71f1470962019bd89534a2919f5925f93bc5779"

cache_root="${WHILEFRESH_CACHE_DIR:-$HOME/.cache/whilefresh}"

fetch_pinned() {
    local url="$1" commit="$2" dir="$3"
    if [[ -d "$dir/.git" ]] && [[ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" == "$commit" ]]; then
        echo "up to date: $dir @ $commit"
        return
    fi
    mkdir -p "$dir"
    if [[ ! -d "$dir/.git" ]]; then
        git -C "$dir" init -q
        git -C "$dir" remote add origin "$url"
    fi
    git -C "$dir" fetch -q --depth 1 origin "$commit"
    git -C "$dir" checkout -q --detach FETCH_HEAD
    echo "fetched: $dir @ $commit"
}

fetch_pinned "$ANDROID_OPENSSL_URL" "$ANDROID_OPENSSL_COMMIT" "$cache_root/android_openssl"
