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

"$emulator" -accel-check || echo "warning: hardware acceleration unavailable, the emulator will be slow" >&2

nohup "$emulator" -avd "$avd" -no-snapshot-save -no-boot-anim "${extra_args[@]+"${extra_args[@]}"}" \
    > "$log" 2>&1 &
echo "emulator starting (pid $!), log: $log"

"$ADB" wait-for-device
for _ in $(seq 1 180); do
    if [[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; then
        echo "emulator booted"
        exit 0
    fi
    sleep 2
done
echo "emulator did not finish booting within 6 minutes; see $log" >&2
exit 1
