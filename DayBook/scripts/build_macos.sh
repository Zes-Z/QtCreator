#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_ROOT="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

for tools_dir in "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin" "$HOME/Qt/Tools/Ninja"; do
    if [ -d "$tools_dir" ]; then
        export PATH="$tools_dir:$PATH"
    fi
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found. Install CMake or set PATH to Qt's CMake." >&2
    exit 1
fi
if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja not found. Install Ninja or set PATH to Qt's Ninja." >&2
    exit 1
fi
if [ ! -d "$QT_ROOT" ]; then
    echo "Qt not found at $QT_ROOT. Set QT_ROOT to your Qt 6.12 macOS directory." >&2
    exit 1
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR"

echo
echo "Built: $BUILD_DIR/DayBook.app"
echo "Run:   $BUILD_DIR/DayBook.app/Contents/MacOS/DayBook"
