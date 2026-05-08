# Razer Mouse Monitor

A small native Qt/C++ Linux app for viewing battery and mouse telemetry from a Razer DeathAdder V3 HyperSpeed wireless receiver.

Built for Fedora, but the code is plain Linux/Qt and may work elsewhere.

## What it shows

- Mouse/device name and USB IDs
- Battery percentage
- Charging state, when available
- DPI and polling rate, when available
- KDE mouse acceleration settings
- Basic diagnostics when Linux blocks access

The app talks directly to the Razer USB receiver while it is open. It does not install OpenRazer, start a daemon, or keep anything running after you close the app.

## Download

Grab the AppImage from the releases page:

```text
Razer_Mouse_Monitor-x86_64.AppImage
```

Make it executable and run it:

```bash
chmod +x Razer_Mouse_Monitor-x86_64.AppImage
./Razer_Mouse_Monitor-x86_64.AppImage
```

## Required permission rule

Fedora usually makes `/dev/hidraw*` devices root-only. Battery/DPI telemetry needs access to the Razer receiver's hidraw node.

Install the included udev rule:

```bash
./install-hidraw-permissions.sh
```

Then unplug/replug the Razer USB receiver and restart the app.

The rule is narrow: it applies only to Razer vendor `1532`, product `00c5`. It does not install packages, run a service, or grant access to all USB devices.

To remove it:

```bash
sudo rm /etc/udev/rules.d/70-razer-deathadder-v3-hyperspeed-hidraw.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then unplug/replug the receiver.

## Build from source

Install dependencies:

```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel upower glib2 systemd-udev ImageMagick curl
```

Build and run:

```bash
./run.sh
```

Build an AppImage:

```bash
./scripts/build-appimage.sh
```

Output:

```text
dist/Razer_Mouse_Monitor-x86_64.AppImage
```

## Notes

- The DeathAdder V3 HyperSpeed receiver is 2.4 GHz USB, not Bluetooth.
- The AppImage still needs the hidraw permission rule. AppImage packaging cannot bypass Linux device permissions.
- Telemetry support is currently targeted at USB product ID `1532:00c5`.

## License

GPL-2.0-or-later.

The Razer HID report format and device constants were implemented from the GPL-licensed OpenRazer project and Linux hidraw APIs.
