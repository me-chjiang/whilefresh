#!/usr/bin/env bash
# Start the fm_api34 emulator in the background and wait until Android has booted.
# Usage: scripts/run-emulator.sh [--headless]
# Stop it with: adb emu kill
set -euo pipefail
source "$(dirname "$0")/env.sh"

avd="${WHILEFRESH_AVD:-fm_api34}"
emulator="$ANDROID_SDK_ROOT/emulator/emulator"
log="${TMPDIR:-/tmp}/whilefresh-emulator.log"

extra_args=()
for arg in "$@"; do
    case "$arg" in
        --headless) extra_args+=(-no-window) ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

if "$ADB" devices | grep '^emulator-' >/dev/null; then
    echo "an emulator is already attached; stop it first with: adb emu kill" >&2
    exit 1
fi

"$emulator" -accel-check || echo "warning: hardware acceleration unavailable, the emulator will be slow" >&2

nohup "$emulator" -avd "$avd" -no-snapshot -no-boot-anim \
    -feature -Vulkan -gpu swiftshader_indirect "${extra_args[@]+"${extra_args[@]}"}" \
    > "$log" 2>&1 &
pid=$!
echo "emulator starting (pid $pid), log: $log"

# Poll instead of `adb wait-for-device`, which would block forever if the emulator dies.
for _ in $(seq 1 180); do
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "emulator exited before boot completed; last log lines:" >&2
        tail -n 20 "$log" >&2
        exit 1
    fi
    if [[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; then
        echo "emulator booted"
        exit 0
    fi
    sleep 2
done
echo "emulator did not finish booting within 6 minutes; see $log" >&2
exit 1
