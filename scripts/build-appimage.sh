#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

APP=razer-mouse-monitor
APPDIR="$ROOT/dist/AppDir"
TOOLS="$ROOT/.appimage-tools"
ARCH_NAME="$(uname -m)"

case "$ARCH_NAME" in
  x86_64|amd64) APPIMAGE_ARCH=x86_64 ;;
  aarch64|arm64) APPIMAGE_ARCH=aarch64 ;;
  *) echo "Unsupported AppImage architecture: $ARCH_NAME" >&2; exit 1 ;;
esac

mkdir -p "$TOOLS" "$ROOT/dist"

LINUXDEPLOY="$TOOLS/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
APPIMAGETOOL="$TOOLS/appimagetool-${APPIMAGE_ARCH}.AppImage"

if [[ ! -x "$LINUXDEPLOY" ]]; then
  curl -L --fail -o "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
  chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$APPIMAGETOOL" ]]; then
  curl -L --fail -o "$APPIMAGETOOL" "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage"
  chmod +x "$APPIMAGETOOL"
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install build

cp packaging/${APP}.desktop "$APPDIR/${APP}.desktop"
cp assets/razer-mouse-monitor.png "$APPDIR/${APP}.png"
ln -sf "${APP}.png" "$APPDIR/.DirIcon"

# Bundle ELF dependencies. Current Fedora libraries can use newer ELF sections
# than linuxdeploy's embedded strip understands, so strip failures are allowed;
# linuxdeploy copies the libraries before hitting that problem.
export APPIMAGE_EXTRACT_AND_RUN=1
set +e
"$LINUXDEPLOY" --appdir "$APPDIR" --desktop-file "$APPDIR/${APP}.desktop" --icon-file "$APPDIR/${APP}.png"
LINUXDEPLOY_STATUS=$?
set -e
if [[ $LINUXDEPLOY_STATUS -ne 0 ]]; then
  echo "linuxdeploy returned $LINUXDEPLOY_STATUS after dependency collection; continuing with manual AppImage packaging."
fi

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "${0}")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$HERE/usr/plugins:${QT_PLUGIN_PATH:-}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/usr/plugins/platforms:${QT_QPA_PLATFORM_PLUGIN_PATH:-}"
exec "$HERE/usr/bin/razer-mouse-monitor" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

mkdir -p "$APPDIR/usr/plugins/platforms" "$APPDIR/usr/plugins/imageformats" \
  "$APPDIR/usr/plugins/wayland-decoration-client" "$APPDIR/usr/plugins/wayland-shell-integration"
cp -a /usr/lib64/qt6/plugins/platforms/libqxcb.so "$APPDIR/usr/plugins/platforms/" 2>/dev/null || true
cp -a /usr/lib64/qt6/plugins/platforms/libqwayland-generic.so "$APPDIR/usr/plugins/platforms/" 2>/dev/null || true
cp -a /usr/lib64/qt6/plugins/imageformats/libqpng.so "$APPDIR/usr/plugins/imageformats/" 2>/dev/null || true
cp -a /usr/lib64/qt6/plugins/wayland-decoration-client/* "$APPDIR/usr/plugins/wayland-decoration-client/" 2>/dev/null || true
cp -a /usr/lib64/qt6/plugins/wayland-shell-integration/* "$APPDIR/usr/plugins/wayland-shell-integration/" 2>/dev/null || true

OUT="$ROOT/dist/Razer_Mouse_Monitor-${APPIMAGE_ARCH}.AppImage"
rm -f "$OUT"
ARCH="$APPIMAGE_ARCH" APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" "$APPDIR" "$OUT"
ls -lh "$OUT"
