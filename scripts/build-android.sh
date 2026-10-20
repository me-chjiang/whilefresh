#!/usr/bin/env bash
# Build a debug-signed Android APK for one ABI into build/android-<abi>.
# Usage: scripts/build-android.sh <x86_64|arm64-v8a> [--no-key]
# The DeepSeek key is read from the macOS keychain unless --no-key is given (ADR-5).
set -euo pipefail
source "$(dirname "$0")/env.sh"

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <x86_64|arm64-v8a> [--no-key]" >&2
    exit 2
fi

abi="$1"
shift
case "$abi" in
    x86_64) kit=android_x86_64 ;;
    arm64-v8a) kit=android_arm64_v8a ;;
    *) echo "unsupported ABI: $abi (expected x86_64 or arm64-v8a)" >&2; exit 2 ;;
esac

with_key=1
for arg in "$@"; do
    case "$arg" in
        --no-key) with_key=0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

"$REPO_ROOT/scripts/fetch-deps.sh"

if [[ $with_key -eq 1 ]]; then
    export_deepseek_key
else
    disable_deepseek_key
fi

build_dir="$REPO_ROOT/build/android-$abi"
"$QT_ROOT/$kit/bin/qt-cmake" -S "$REPO_ROOT/app" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH="$QT_ROOT/macos" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DWHILEFRESH_ANDROID_OPENSSL_DIR="$WHILEFRESH_ANDROID_OPENSSL_DIR"
cmake --build "$build_dir" --target apk

apk="$(find "$build_dir/android-build/build/outputs/apk/debug" -name "*.apk" -print -quit)"
if [[ -z "$apk" ]]; then
    echo "APK not found under $build_dir/android-build" >&2
    exit 1
fi
cp "$apk" "$build_dir/whilefresh-$abi-debug.apk"
echo "built: $build_dir/whilefresh-$abi-debug.apk"
