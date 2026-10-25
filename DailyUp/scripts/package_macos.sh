#!/usr/bin/env bash
# 构建 Release、打包 .app、ad-hoc 签名并生成 DMG。
set -euo pipefail

cd "$(dirname "$0")/.."

QT_ROOT="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
BUILD_DIR="${BUILD_DIR:-build/release}"
VERSION="${VERSION:-1.0.0}"
APP_NAME="DailyUp"
DIST="dist"

if [[ ! -x "$QT_ROOT/bin/macdeployqt" ]]; then
    echo "找不到 macdeployqt：$QT_ROOT/bin/macdeployqt" >&2
    exit 1
fi

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
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR"

rm -rf "$DIST/$APP_NAME.app" "$DIST/$APP_NAME-$VERSION.dmg"
mkdir -p "$DIST"
cp -R "$BUILD_DIR/$APP_NAME.app" "$DIST/"

"$QT_ROOT/bin/macdeployqt" "$DIST/$APP_NAME.app" \
    -always-overwrite \
    -appstore-compliant \
    -qmldir="$PWD"

# 只保留 SQLite 驱动，避免未使用的 ODBC / Mimer 驱动带来缺失依赖。
rm -f "$DIST/$APP_NAME.app/Contents/PlugIns/sqldrivers/libqsqlmimer.dylib"

codesign --force --deep --sign - "$DIST/$APP_NAME.app"
codesign --verify --deep --strict "$DIST/$APP_NAME.app"

STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
cp -R "$DIST/$APP_NAME.app" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

if hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$STAGING" \
    -ov -format UDZO \
    "$DIST/$APP_NAME-$VERSION.dmg"; then
    PACKAGE="$DIST/$APP_NAME-$VERSION.dmg"
else
    # 某些受限环境（如 CI / 沙箱）不允许挂载临时卷，回退到 zip。
    echo "DMG 创建失败，改用 zip 打包。"
    rm -f "$DIST/$APP_NAME-$VERSION-macos.zip"
    ditto -c -k --sequesterRsrc --keepParent \
        "$DIST/$APP_NAME.app" \
        "$DIST/$APP_NAME-$VERSION-macos.zip"
    PACKAGE="$DIST/$APP_NAME-$VERSION-macos.zip"
fi

echo
echo "应用：$DIST/$APP_NAME.app"
echo "安装包：$PACKAGE"
echo
echo "分发提示：面向他人分发时请改用 Developer ID 签名并完成 notarization。"