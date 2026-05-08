#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP=razer-mouse-monitor
APPIMAGE="$ROOT/dist/Razer_Mouse_Monitor-x86_64.AppImage"
BINARY="$ROOT/build/razer-mouse-monitor"
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICON_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
DESKTOP_FILE="$DESKTOP_DIR/$APP.desktop"

mkdir -p "$DESKTOP_DIR"

if [[ -x "$APPIMAGE" ]]; then
  EXEC="$APPIMAGE"
elif [[ -x "$BINARY" ]]; then
  EXEC="$BINARY"
else
  echo "No runnable app found. Run ./run.sh or ./scripts/build-appimage.sh first." >&2
  exit 1
fi

mkdir -p "$ICON_DIR"
cp -a "$ROOT/assets/icons/hicolor/"* "$ICON_DIR/"

cat > "$DESKTOP_FILE" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Razer Mouse Monitor
GenericName=Mouse Monitor
Comment=Show Razer DeathAdder battery, DPI, polling rate, and Linux mouse settings
Exec="$EXEC"
Icon=razer-mouse-monitor
Terminal=false
Categories=Settings;HardwareSettings;Qt;
Keywords=razer;razor;mouse;deathadder;battery;dpi;polling;hidraw;
StartupNotify=true
DESKTOP

chmod 0644 "$DESKTOP_FILE"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q "$ICON_DIR" >/dev/null 2>&1 || true
fi
if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
elif command -v kbuildsycoca5 >/dev/null 2>&1; then
  kbuildsycoca5 --noincremental >/dev/null 2>&1 || true
fi

echo "Installed desktop entry: $DESKTOP_FILE"
echo "KRunner should find it with: razer, razor, mouse, battery, dpi"
