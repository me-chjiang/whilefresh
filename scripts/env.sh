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
WHILEFRESH_YAML_CPP_DIR="${WHILEFRESH_YAML_CPP_DIR:-${WHILEFRESH_CACHE_DIR:-$HOME/.cache/whilefresh}/yaml-cpp}"
export ANDROID_SDK_ROOT ANDROID_NDK_ROOT JAVA_HOME WHILEFRESH_ANDROID_OPENSSL_DIR WHILEFRESH_YAML_CPP_DIR

# Exports WHILEFRESH_DEEPSEEK_API_KEY from the macOS keychain (ADR-5), or an empty value.
# Never print the key; only report whether it was found.
export_deepseek_key() {
    local key
    key="$(security find-generic-password -s landing-project-deepseek -w 2>/dev/null || true)"
    export WHILEFRESH_DEEPSEEK_API_KEY="$key" WHILEFRESH_KEY_FROM_SCRIPT=1
    if [[ -n "$key" ]]; then
        echo "DeepSeek API key: read from keychain"
    else
        echo "DeepSeek API key: not found in keychain (service landing-project-deepseek), building without it"
    fi
}

disable_deepseek_key() {
    export WHILEFRESH_DEEPSEEK_API_KEY="" WHILEFRESH_KEY_FROM_SCRIPT=1
}

LEXMOUNT_CONFIG="${WHILEFRESH_LEXMOUNT_CONFIG:-$HOME/.config/lexmount/config.yaml}"

# Prints the value of a top-level `name: value` line of the Lexmount config (quotes stripped).
lexmount_config_value() {
    [[ -f "$LEXMOUNT_CONFIG" ]] || return 0
    sed -n -E "s/^$1:[[:space:]]*//p" "$LEXMOUNT_CONFIG" | head -n 1 \
        | sed -E "s/[[:space:]]+#.*$//; s/^[\"']//; s/[\"'][[:space:]]*$//; s/[[:space:]]+$//"
}

# Exports WHILEFRESH_LEXMOUNT_API_KEY and WHILEFRESH_LEXMOUNT_PROJECT_ID from the Lexmount config
# (ADR-5, same rules as the DeepSeek key): never print the values, only whether they were found.
export_lexmount_credentials() {
    local key project
    key="$(lexmount_config_value api-key)"
    project="$(lexmount_config_value project-id)"
    export WHILEFRESH_LEXMOUNT_API_KEY="$key" WHILEFRESH_LEXMOUNT_PROJECT_ID="$project"
    if [[ -n "$key" && -n "$project" ]]; then
        echo "Lexmount credentials: read from $LEXMOUNT_CONFIG"
    else
        echo "Lexmount credentials: not found in $LEXMOUNT_CONFIG, building without them"
        export WHILEFRESH_LEXMOUNT_API_KEY="" WHILEFRESH_LEXMOUNT_PROJECT_ID=""
    fi
}

disable_lexmount_credentials() {
    export WHILEFRESH_LEXMOUNT_API_KEY="" WHILEFRESH_LEXMOUNT_PROJECT_ID=""
}

require_path() {
    local path="$1" what="$2" override="$3"
    if [[ ! -e "$path" ]]; then
        echo "error: $what not found at $path (override with $override)" >&2
        exit 1
    fi
}

require_path "$QT_ROOT" "Qt 6.8.2 install" WHILEFRESH_QT_ROOT
require_path "$ANDROID_SDK_ROOT" "Android SDK" ANDROID_SDK_ROOT
require_path "$ANDROID_NDK_ROOT" "Android NDK" ANDROID_NDK_ROOT
require_path "$JAVA_HOME/bin/java" "JDK 17" JAVA_HOME
