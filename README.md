# Razer Mouse Monitor

Native Qt/C++ utility for Fedora Linux that shows local mouse status for a Razer DeathAdder V3 HyperSpeed receiver (`1532:00c5`).

It is intentionally app-local: no daemon, no background service, and no OpenRazer install. The app opens the Razer hidraw device during refresh, asks read-only telemetry questions, then closes it.

## Features

- Detects Razer USB/HID device name, vendor/product IDs, kernel path, and input endpoints
- Reads battery percentage through direct hidraw for `1532:00c5`
- Reads charging state, DPI, and polling rate when the receiver responds to those direct HID queries
- Shows KDE mouse acceleration config first, with GNOME/xinput fallback
- Lists Bluetooth devices only as diagnostics; the DeathAdder V3 HyperSpeed receiver is 2.4 GHz USB, not Bluetooth
- Includes a generated desktop icon in `assets/`
- Shows diagnostics for missing permissions or unsupported telemetry

## Fedora dependencies

On a fresh Fedora install:

```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel upower glib2 systemd-udev ImageMagick curl
```

## Build and run from source

```bash
./run.sh
```

## Build an AppImage

```bash
./scripts/build-appimage.sh
```

Output:

```text
dist/Razer_Mouse_Monitor-x86_64.AppImage
```

Run it:

```bash
./dist/Razer_Mouse_Monitor-x86_64.AppImage
```

The AppImage still needs the same hidraw permission rule below to read battery/DPI data. Packaging an app as an AppImage does not magically bypass Linux permissions. Annoying, but correct.

## hidraw permissions

Fedora may create Razer hidraw nodes as root-only. If the app says permission denied for `/dev/hidraw*`, install the narrow udev rule:

```bash
./install-hidraw-permissions.sh
# unplug/replug the Razer USB receiver
./run.sh
```

The rule applies only to Razer vendor `1532` and product `00c5` hidraw devices. It does not install packages, start daemons, or grant access to every USB device.

To remove the rule:

```bash
sudo rm /etc/udev/rules.d/70-razer-deathadder-v3-hyperspeed-hidraw.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
# unplug/replug the receiver
```

## Security note

The udev rule lets programs running in your logged-in desktop session open this specific mouse receiver's hidraw nodes. That is a small but real local-user trust tradeoff. It is narrower than broad `plugdev` or `0666` access.

## Release with GitHub CLI

After committing:

```bash
gh repo create razer-mouse-monitor --public --source=. --remote=origin --push
./scripts/build-appimage.sh
gh release create v0.1.0 dist/Razer_Mouse_Monitor-x86_64.AppImage --title "v0.1.0" --notes "Initial AppImage build."
```

## Public repo notes

Generated build output and local AppImage tooling are ignored. Do not commit `build/`, `.appimage-tools/`, `dist/AppDir/`, local IDE folders, logs, or screenshots.

## License and protocol note

This project is licensed under GPL-2.0-or-later. The Razer HID report format and device constants were implemented from the GPL-licensed OpenRazer project and Linux hidraw APIs.

## Icon

The app icon was generated with the built-in image generation tool, processed locally to remove the chroma-key background, and resized into Linux icon sizes under `assets/icons/hicolor/`.
