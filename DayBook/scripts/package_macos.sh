#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_ROOT="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
DIST="${DIST:-$ROOT/dist}"
APP_NAME="DayBook"
VERSION="1.0.0"

"$ROOT/scripts/build_macos.sh"

MACDEPLOYQT="$QT_ROOT/bin/macdeployqt"
if [ ! -x "$MACDEPLOYQT" ]; then
    echo "macdeployqt not found at $MACDEPLOYQT" >&2
    exit 1
fi

rm -rf "$DIST/$APP_NAME.app"
mkdir -p "$DIST"
ditto "$BUILD_DIR/$APP_NAME.app" "$DIST/$APP_NAME.app"

"$MACDEPLOYQT" "$DIST/$APP_NAME.app" -always-overwrite -qmldir="$ROOT/qml"

# DayBook only needs the SQLite driver. Drop optional SQL plugins that may
# point at libraries which are not installed on this machine.
SQLPLUGINS="$DIST/$APP_NAME.app/Contents/PlugIns/sqldrivers"
rm -f "$SQLPLUGINS/libqsqlodbc.dylib" "$SQLPLUGINS/libqsqlpsql.dylib" "$SQLPLUGINS/libqsqlmimer.dylib" "$SQLPLUGINS/libqsqlibase.dylib"

# Ad-hoc signature for local use. Replace "-" with your Developer ID for
# notarized distribution.
codesign --force --deep --sign - "$DIST/$APP_NAME.app"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
ditto "$DIST/$APP_NAME.app" "$STAGE/$APP_NAME.app"
ln -s /Applications "$STAGE/Applications"

DMG="$DIST/$APP_NAME-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -volname "$APP_NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null

echo
echo "App:  $DIST/$APP_NAME.app"
echo "DMG:  $DMG"
