#!/bin/sh
set -eu

BIN="$1"
ASSETS="$2"
DESKTOP="$3"
ICON="$4"
DIST="$5"
ASSET_SUBDIR="$6"
VERSION="$7"

ARCH="$(uname -m)"
WORK="$DIST/appimage-work"
APPDIR="$WORK/AppDir"
TOOLS="$WORK/tools"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/$ASSET_SUBDIR" "$TOOLS"

cp "$BIN" "$APPDIR/usr/bin/nekodrome"
cp -r "$ASSETS"/. "$APPDIR/$ASSET_SUBDIR/"
cp "$ICON" "$WORK/nekodrome.png"

LINUXDEPLOY="${LINUXDEPLOY:-$TOOLS/linuxdeploy-$ARCH.AppImage}"
PLUGIN="$TOOLS/linuxdeploy-plugin-appimage-$ARCH.AppImage"

if [ ! -x "$LINUXDEPLOY" ]; then
  curl -fL -o "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
  chmod +x "$LINUXDEPLOY"
fi

if [ ! -x "$PLUGIN" ]; then
  curl -fL -o "$PLUGIN" "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-$ARCH.AppImage"
  chmod +x "$PLUGIN"
fi

export PATH="$TOOLS:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1
export VERSION
export OUTPUT="nekodrome-$VERSION-$ARCH.AppImage"

cd "$DIST"
"$LINUXDEPLOY" \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/nekodrome" \
  --desktop-file "$DESKTOP" \
  --icon-file "$WORK/nekodrome.png" \
  --output appimage
