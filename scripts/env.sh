#!/usr/bin/env bash
# Shared toolchain locations for the build scripts. Source it; do not run it.
# Every path can be overridden from the environment.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_ROOT="${WHILEFRESH_QT_ROOT:-$HOME/Qt/6.8.2}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/26.1.10909125}"
JAVA_HOME="${JAVA_HOME:-$HOME/Library/Java/JavaVirtualMachines/jdk-17.0.20.1+1/Contents/Home}"
WHILEFRESH_ANDROID_OPENSSL_DIR="${WHILEFRESH_ANDROID_OPENSSL_DIR:-${WHILEFRESH_CACHE_DIR:-$HOME/.cache/whilefresh}/android_openssl}"
ANDROID_PACKAGE="io.github.mechjiang.whilefresh"
ADB="$ANDROID_SDK_ROOT/platform-tools/adb"
export ANDROID_SDK_ROOT ANDROID_NDK_ROOT JAVA_HOME WHILEFRESH_ANDROID_OPENSSL_DIR

# Exports WHILEFRESH_DEEPSEEK_API_KEY from the macOS keychain (ADR-5), or an empty value.
# Never print the key; only report whether it was found.
export_deepseek_key() {
    local key
    key="$(security find-generic-password -s landing-project-deepseek -w 2>/dev/null || true)"
    export WHILEFRESH_DEEPSEEK_API_KEY="$key"
    if [[ -n "$key" ]]; then
        echo "DeepSeek API key: read from keychain"
    else
        echo "DeepSeek API key: not found in keychain (service landing-project-deepseek), building without it"
    fi
}

disable_deepseek_key() {
    export WHILEFRESH_DEEPSEEK_API_KEY=""
}
