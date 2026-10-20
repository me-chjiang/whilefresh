#!/usr/bin/env bash
# Install the debug APK on the connected device or emulator and launch it.
# Usage: scripts/install-android.sh [x86_64|arm64-v8a]
# The ABI defaults to the device's primary ABI. With several devices attached, set ANDROID_SERIAL.
set -euo pipefail
source "$(dirname "$0")/env.sh"

abi="${1:-$("$ADB" shell getprop ro.product.cpu.abi | tr -d '\r')}"
apk="$REPO_ROOT/build/android-$abi/whilefresh-$abi-debug.apk"
if [[ ! -f "$apk" ]]; then
    echo "APK not found: $apk (run scripts/build-android.sh $abi first)" >&2
    exit 1
fi

"$ADB" install -r "$apk"
activity="$("$ADB" shell cmd package resolve-activity --brief "$ANDROID_PACKAGE" | tr -d '\r' | tail -n 1)"
"$ADB" shell am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n "$activity"
