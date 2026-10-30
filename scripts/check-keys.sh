#!/usr/bin/env bash
# Pre-commit scan: no DeepSeek-style key and none of the real Lexmount values may appear in the
# working tree or in any commit. Never prints a match's content, only file names. Exit 1 on a hit.
# Usage: scripts/check-keys.sh
set -uo pipefail
source "$(dirname "$0")/env.sh"
cd "$REPO_ROOT"

fail=0
skip=(--glob '!build/**' --glob '!private/pending/**' --glob '!.git/**' --glob '!.claude/worktrees/**')

if rg -uu -l -e 'sk-[0-9a-f]{32}' "${skip[@]}" . >/tmp/check-keys.hits 2>/dev/null; then
    echo "FAIL: sk- style key pattern found in:"; sed 's/^/  /' /tmp/check-keys.hits; fail=1
else
    echo "ok: no sk- key pattern in the working tree"
fi
rm -f /tmp/check-keys.hits

patterns="$(mktemp)"
trap 'rm -f "$patterns"' EXIT
chmod 600 "$patterns"
for name in api-key project-id; do
    value="$(lexmount_config_value "$name")"
    [[ -n "$value" ]] && printf '%s\n' "$value" >>"$patterns"
done

if [[ ! -s "$patterns" ]]; then
    echo "skipped: no Lexmount values found in $LEXMOUNT_CONFIG"
else
    if rg -uu -l -F -f "$patterns" "${skip[@]}" . >/tmp/check-keys.hits 2>/dev/null; then
        echo "FAIL: Lexmount value found in the working tree:"; sed 's/^/  /' /tmp/check-keys.hits; fail=1
    else
        echo "ok: no Lexmount value in the working tree"
    fi
    rm -f /tmp/check-keys.hits
    if git grep -l -F -f "$patterns" $(git rev-list --all) -- >/tmp/check-keys.hits 2>/dev/null; then
        echo "FAIL: Lexmount value found in commit history (commit:path):"; sed 's/^/  /' /tmp/check-keys.hits; fail=1
    else
        echo "ok: no Lexmount value in any commit"
    fi
    rm -f /tmp/check-keys.hits
fi
exit $fail
