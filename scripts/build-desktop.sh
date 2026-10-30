#!/usr/bin/env bash
# Build the macOS desktop development version into build/desktop.
# Usage: scripts/build-desktop.sh [--with-key] [--release]
set -euo pipefail
source "$(dirname "$0")/env.sh"

build_type=Debug
with_key=0
for arg in "$@"; do
    case "$arg" in
        --with-key) with_key=1 ;;
        --release) build_type=Release ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [[ $with_key -eq 1 ]]; then
    export_deepseek_key
    export_lexmount_credentials
else
    disable_deepseek_key
    disable_lexmount_credentials
fi

build_dir="$REPO_ROOT/build/desktop"
"$QT_ROOT/macos/bin/qt-cmake" -S "$REPO_ROOT/app" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir"

echo "built: $build_dir/whilefresh"
