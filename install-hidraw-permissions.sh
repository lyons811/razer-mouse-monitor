#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
  echo "Run this as your normal user, not root. It uses sudo only to install the udev rule."
  exit 1
fi

rule_file=/etc/udev/rules.d/70-razer-deathadder-v3-hyperspeed-hidraw.rules
rule='KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1532", ATTRS{idProduct}=="00c5", TAG+="uaccess", MODE="0660"'

echo "Installing udev permission rule for Razer DeathAdder V3 HyperSpeed wireless receiver (1532:00c5)."
echo "This does not install or start a daemon. It only lets your logged-in desktop user open the mouse hidraw node."
echo "$rule" | sudo tee "$rule_file" >/dev/null
sudo udevadm control --reload-rules
sudo udevadm trigger

echo
printf 'Rule installed at %s\n' "$rule_file"
echo "Unplug/replug the Razer USB receiver, then restart ./run.sh."
