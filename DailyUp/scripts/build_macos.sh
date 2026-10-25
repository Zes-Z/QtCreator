#!/usr/bin/env bash
# 在 macOS 上构建 DailyUp（Debug 或 Release）。
# 用法：./scripts/build_macos.sh [Debug|Release]
set -euo pipefail

cd "$(dirname "$0")/.."

CONFIG="${1:-${CONFIG:-Debug}}"
QT_ROOT="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
BUILD_DIR="${BUILD_DIR:-build/macos}"

if [[ ! -d "$QT_ROOT" ]]; then
    echo "找不到 Qt: $QT_ROOT" >&2
    echo "请设置 QT_ROOT，例如：QT_ROOT=\$HOME/Qt/6.12.0/macos ./scripts/build_macos.sh" >&2
    exit 1
fi

# Qt 自带的 CMake / Ninja 不在系统 PATH 中时优先使用
if [[ -d "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin" ]]; then
    export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$PATH"
fi
if [[ -d "$HOME/Qt/Tools/Ninja" ]]; then
    export PATH="$HOME/Qt/Tools/Ninja:$PATH"
fi

GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=(-G Ninja)
fi

cmake -S . -B "$BUILD_DIR" "${GENERATOR[@]}" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DCMAKE_BUILD_TYPE="$CONFIG"

cmake --build "$BUILD_DIR"

echo
echo "已构建：$BUILD_DIR/DailyUp.app"
echo "运行：open \"$BUILD_DIR/DailyUp.app\""
