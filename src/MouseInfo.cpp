#include "MouseInfo.h"
#include "RazerHidBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

static constexpr const char *RAZER_VENDOR = "1532";

QString MouseInfoCollector::readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

QString MouseInfoCollector::runCommand(const QString &program, const QStringList &args, int timeoutMs) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(250)) return {};
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(250);
        return {};
    }
    QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    return out.isEmpty() ? err : out;
}

QStringList MouseInfoCollector::listDirs(const QString &path) {
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList out;
    for (const QFileInfo &fi : entries) out << fi.absoluteFilePath();
    return out;
}

bool MouseInfoCollector::textMatchesRazerMouse(const QString &text) {
    const QString t = text.toLower();
    return t.contains("razer") || t.contains("deathadder") || t.contains(RAZER_VENDOR);
}

QString MouseInfoCollector::normalizePercent(const QString &value) {
    QString v = value.trimmed();
    if (v.isEmpty()) return {};
    if (!v.endsWith('%')) v += "%";
    return v;
}

DeviceInfo MouseInfoCollector::parseBluetoothctlDevice(const QString &deviceLine, const QString &infoText, QStringList *diagnostics) {
    DeviceInfo info;

    QRegularExpression lineRe("^Device\\s+([0-9A-Fa-f:]{17})\\s+(.+)$");
    const auto lineMatch = lineRe.match(deviceLine.trimmed());
    if (!lineMatch.hasMatch()) return info;

    const QString mac = lineMatch.captured(1).toUpper();
    const QString lineName = lineMatch.captured(2).trimmed();
    const QString name = [&]() {
        QRegularExpression nameRe("\\n\\s*Name:\\s*([^\\n]+)");
        QRegularExpression aliasRe("\\n\\s*Alias:\\s*([^\\n]+)");
        auto nm = nameRe.match("\n" + infoText);
        if (nm.hasMatch()) return nm.captured(1).trimmed();
        auto am = aliasRe.match("\n" + infoText);
        if (am.hasMatch()) return am.captured(1).trimmed();
        return lineName;
    }();

    const QString combined = (deviceLine + "\n" + infoText + "\n" + name).toLower();
    const bool looksLikeMouse = combined.contains("input-mouse") ||
                                combined.contains("mouse") ||
                                combined.contains("human interface device");
    const bool looksLikeRazer = textMatchesRazerMouse(combined);
    if (!looksLikeRazer && !looksLikeMouse) return info;
    if (!looksLikeRazer) return info;

    QRegularExpression connectedRe("\\n\\s*Connected:\\s*(yes|no)", QRegularExpression::CaseInsensitiveOption);
    const auto connected = connectedRe.match("\n" + infoText);
    const bool isConnected = connected.hasMatch() && connected.captured(1).compare("yes", Qt::CaseInsensitive) == 0;

    QRegularExpression modaliasRe("Modalias:\\s*usb:v([0-9A-Fa-f]{4})p([0-9A-Fa-f]{4})");
    const auto modalias = modaliasRe.match(infoText);

    info.found = true;
    info.name = name.isEmpty() ? lineName : name;
    info.manufacturer = looksLikeRazer ? "Razer" : "Bluetooth";
    info.connection = isConnected ? "Bluetooth · connected" : "Bluetooth · paired";
    info.kernelPath = "bluetooth://" + mac;
    if (modalias.hasMatch()) {
        info.vendorId = modalias.captured(1).toLower();
        info.productId = modalias.captured(2).toLower();
    }
    info.hidNames << info.name;
    info.usbDetails << "Bluetooth MAC: " + mac;

    QRegularExpression batteryRe("Battery Percentage:\\s*0x[0-9A-Fa-f]+\\s*\\((\\d+)\\)");
    const auto bm = batteryRe.match(infoText);
    if (bm.hasMatch()) info.usbDetails << "Bluetooth battery: " + bm.captured(1) + "%";

    return info;
}

DeviceInfo MouseInfoCollector::parseInputUevent(const QString &eventPath, const QString &ueventText) {
    DeviceInfo info;
    QRegularExpression productRe("PRODUCT=([0-9a-fA-F]+)/([0-9a-fA-F]{4})/([0-9a-fA-F]+)/");
    QRegularExpression nameRe("NAME=\\\"?([^\\n\\\"]+)\\\"?");
    const auto product = productRe.match(ueventText);
    const auto name = nameRe.match(ueventText);
    if (!product.hasMatch()) return info;

    const QString vendor = product.captured(2).toLower().rightJustified(4, '0');
    const QString productId = product.captured(3).toLower().rightJustified(4, '0');
    const QString deviceName = name.hasMatch() ? name.captured(1).trimmed() : QString();
    const QString combined = (ueventText + "\n" + deviceName).toLower();
    if (vendor != RAZER_VENDOR && !textMatchesRazerMouse(combined)) return info;

    info.found = true;
    info.name = deviceName.isEmpty() ? "Razer input device" : deviceName;
    info.vendorId = vendor;
    info.productId = productId;
    info.manufacturer = "Razer";
    info.connection = "USB HyperSpeed receiver / input";
    info.kernelPath = eventPath;
    if (!deviceName.isEmpty()) info.hidNames << deviceName;
    return info;
}

PointerSettings MouseInfoCollector::parseKdeMouseConfig(const QString &configText) {
    PointerSettings p;
    QHash<QString, QString> mouse;
    QString group;
    const QStringList lines = configText.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            group = line.mid(1, line.size() - 2);
            continue;
        }
        if (group != "Mouse") continue;
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        mouse.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }

    if (mouse.isEmpty()) return p;
    p.source = "KDE kcminputrc";
    p.desktop = "KDE";
    p.speed = mouse.value("XLbInptPointerAcceleration");
    if (p.speed.isEmpty()) p.speed = mouse.value("PointerAcceleration");
    if (p.speed.isEmpty()) p.speed = "default";

    if (mouse.value("X11LibInputXAccelProfileFlat").compare("true", Qt::CaseInsensitive) == 0) {
        p.accelProfile = "flat";
    } else if (mouse.value("X11LibInputXAccelProfileAdaptive").compare("true", Qt::CaseInsensitive) == 0) {
        p.accelProfile = "adaptive";
    } else {
        p.accelProfile = "default";
    }

    const QString natural = mouse.value("NaturalScroll");
    p.naturalScroll = natural.isEmpty() ? "default" : natural;

    const QString mapping = mouse.value("MouseButtonMapping");
    if (mapping.compare("LeftHanded", Qt::CaseInsensitive) == 0) p.leftHanded = "true";
    else if (mapping.compare("RightHanded", Qt::CaseInsensitive) == 0) p.leftHanded = "false";
    else p.leftHanded = "default";

    return p;
}

DeviceInfo MouseInfoCollector::collectDevice(QStringList &diagnostics) const {
    DeviceInfo info;
    QStringList bluetoothSeen;

    const QString btDevices = runCommand("bluetoothctl", {"devices"}, 1600);
    if (!btDevices.isEmpty()) {
        const QStringList lines = btDevices.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QRegularExpression macRe("^Device\\s+([0-9A-Fa-f:]{17})\\s+(.+)$");
            const auto macMatch = macRe.match(line.trimmed());
            if (!macMatch.hasMatch()) continue;
            bluetoothSeen << macMatch.captured(2).trimmed() + " (" + macMatch.captured(1).toUpper() + ")";
            const QString detail = runCommand("bluetoothctl", {"info", macMatch.captured(1)}, 1600);
            const DeviceInfo bt = parseBluetoothctlDevice(line, detail, &diagnostics);
            if (bt.found && bt.name.toLower().contains("razer")) {
                diagnostics << "Razer Bluetooth device detected via bluetoothctl.";
                return bt;
            }
        }
    }

    QStringList inputNames;
    for (const QString &eventPath : listDirs("/sys/class/input")) {
        if (!QFileInfo(eventPath).fileName().startsWith("event")) continue;
        const DeviceInfo input = parseInputUevent(eventPath, readFile(eventPath + "/device/uevent"));
        if (!input.found) continue;
        inputNames << input.name;
        if (!info.found) {
            info = input;
        } else if (info.vendorId == input.vendorId && info.productId == input.productId) {
            for (const QString &name : input.hidNames) {
                if (!name.isEmpty() && !info.hidNames.contains(name)) info.hidNames << name;
            }
            if (info.name == "Razer mouse" || info.name == "Razer HID mouse") info.name = input.name;
        }
    }

    for (const QString &dev : listDirs("/sys/bus/usb/devices")) {
        const QString vendor = readFile(dev + "/idVendor").toLower();
        if (vendor != RAZER_VENDOR) continue;

        const QString productName = readFile(dev + "/product");
        const QString manufacturer = readFile(dev + "/manufacturer");
        const QString haystack = QStringList{productName, manufacturer, readFile(dev + "/idProduct"), dev}.join(' ');
        if (!haystack.toLower().contains("deathadder") && !productName.toLower().contains("mouse")) {
            // Keep the first Razer USB device as fallback, but prefer explicit mouse names.
            if (info.found) continue;
        }

        info.found = true;
        info.vendorId = vendor;
        info.productId = readFile(dev + "/idProduct").toLower();
        info.name = productName.isEmpty() ? "Razer mouse" : productName;
        info.manufacturer = manufacturer.isEmpty() ? "Razer" : manufacturer;
        info.kernelPath = dev;
        info.connection = readFile(dev + "/speed").isEmpty() ? "USB receiver / HID" : "USB/HID @ " + readFile(dev + "/speed") + " Mbps";
        info.usbDetails << "Bus " + readFile(dev + "/busnum") + ", device " + readFile(dev + "/devnum");
        const QString powerControl = readFile(dev + "/power/control");
        if (!powerControl.isEmpty()) info.usbDetails << "USB power control: " + powerControl;
        break;
    }

    for (const QString &hid : listDirs("/sys/bus/hid/devices")) {
        const QString modalias = readFile(hid + "/modalias").toLower();
        const QString name = readFile(hid + "/name");
        const QString combined = modalias + " " + name.toLower() + " " + hid.toLower();
        if (combined.contains(RAZER_VENDOR) || combined.contains("razer") || combined.contains("deathadder")) {
            if (!name.isEmpty() && !info.hidNames.contains(name)) info.hidNames << name;
            if (!info.found) {
                info.found = true;
                info.name = name.isEmpty() ? "Razer HID mouse" : name;
                info.vendorId = RAZER_VENDOR;
                QRegularExpression re("0003:1532:([0-9a-fA-F]{4})");
                auto match = re.match(hid);
                if (match.hasMatch()) info.productId = match.captured(1).toLower();
                info.manufacturer = "Razer";
                info.kernelPath = hid;
                info.connection = "HID";
            }
        }
    }

    if (!bluetoothSeen.isEmpty()) diagnostics << "Bluetooth devices seen: " + bluetoothSeen.join("; ");
    inputNames.removeDuplicates();
    if (!inputNames.isEmpty()) diagnostics << "Razer input endpoints: " + inputNames.join("; ");
    if (!info.found) diagnostics << "No Razer Bluetooth, HID, or USB mouse was visible. If you meant the DeathAdder V3 HyperSpeed specifically, Razer lists it as HyperSpeed Wireless/wired, not Bluetooth.";
    if (info.found && info.hidNames.isEmpty()) diagnostics << "Razer USB/receiver device was found, but no matching HID name was exposed. This is common with 2.4 GHz receivers.";
    return info;
}

BatteryInfo MouseInfoCollector::collectBattery(const DeviceInfo &device, QStringList &diagnostics) const {
    BatteryInfo battery;
    for (const QString &detail : device.usbDetails) {
        QRegularExpression btBatteryRe("^Bluetooth battery:\\s*(\\d+%)$");
        const auto match = btBatteryRe.match(detail);
        if (match.hasMatch()) {
            battery.available = true;
            battery.source = device.kernelPath;
            battery.capacity = match.captured(1);
            battery.status = "reported by bluetoothctl";
            battery.model = device.name;
            return battery;
        }
    }

    if (device.vendorId.toLower() == RAZER_VENDOR && device.productId.toLower() == "00c5") {
        const RazerTelemetry direct = RazerHidBackend().query();
        diagnostics << direct.diagnostics;
        if (direct.batteryAvailable) {
            battery.available = true;
            battery.source = direct.source.isEmpty() ? "Direct hidraw" : direct.source;
            battery.capacity = normalizePercent(QString::number(direct.batteryPercent));
            if (direct.chargingAvailable) battery.status = direct.charging ? "charging" : "discharging / wireless";
            else battery.status = "reported by direct HID";
            battery.model = device.name;
            return battery;
        }
    }


    for (const QString &ps : listDirs("/sys/class/power_supply")) {
        const QString name = QFileInfo(ps).fileName();
        const QString type = readFile(ps + "/type");
        const QString model = readFile(ps + "/model_name");
        const QString manufacturer = readFile(ps + "/manufacturer");
        const QString scope = readFile(ps + "/scope");
        const QString combined = QStringList{name, type, model, manufacturer, scope, device.name}.join(' ');
        const bool likelyPeripheral = type.compare("Battery", Qt::CaseInsensitive) == 0 &&
                                      (scope.compare("Device", Qt::CaseInsensitive) == 0 || textMatchesRazerMouse(combined));
        if (!likelyPeripheral) continue;
        if (!textMatchesRazerMouse(combined) && !device.name.isEmpty() && !combined.toLower().contains(device.name.toLower().left(8))) continue;

        const QString capacity = readFile(ps + "/capacity");
        if (capacity.isEmpty()) continue;
        battery.available = true;
        battery.source = ps;
        battery.capacity = normalizePercent(capacity);
        battery.status = readFile(ps + "/status");
        battery.model = model.isEmpty() ? name : model;
        return battery;
    }

    const QString upowerList = runCommand("upower", {"-e"});
    if (!upowerList.isEmpty()) {
        const QStringList lines = upowerList.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString lower = line.toLower();
            if (!lower.contains("mouse") && !lower.contains("razer") && !lower.contains("gaming_input")) continue;
            const QString detail = runCommand("upower", {"-i", line.trimmed()});
            if (detail.isEmpty()) continue;
            const QString dlow = detail.toLower();
            if (!dlow.contains("mouse") && !dlow.contains("razer") && !dlow.contains("deathadder")) continue;
            QRegularExpression pctRe("percentage:\\s*([^\\n]+)");
            QRegularExpression stateRe("state:\\s*([^\\n]+)");
            QRegularExpression modelRe("model:\\s*([^\\n]+)");
            auto pm = pctRe.match(detail);
            if (!pm.hasMatch()) continue;
            battery.available = true;
            battery.source = "UPower: " + line.trimmed();
            battery.capacity = pm.captured(1).trimmed();
            auto sm = stateRe.match(detail);
            battery.status = sm.hasMatch() ? sm.captured(1).trimmed() : "reported";
            auto mm = modelRe.match(detail);
            battery.model = mm.hasMatch() ? mm.captured(1).trimmed() : "mouse battery";
            return battery;
        }
    }

    diagnostics << "Battery was not exposed through direct HID, /sys/class/power_supply, or UPower. Check hidraw permissions with ./install-hidraw-permissions.sh.";
    return battery;
}

PointerSettings MouseInfoCollector::collectPointer(const DeviceInfo &device, QStringList &diagnostics) const {
    PointerSettings p;
    p.desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", "unknown");
    RazerTelemetry direct;
    if (device.vendorId.toLower() == RAZER_VENDOR && device.productId.toLower() == "00c5") {
        direct = RazerHidBackend().query();
        diagnostics << direct.diagnostics;
    }

    if (p.desktop.toLower().contains("kde")) {
        const QString kdeConfig = readFile(QDir::homePath() + "/.config/kcminputrc");
        PointerSettings kde = parseKdeMouseConfig(kdeConfig);
        if (!kde.source.isEmpty()) {
            kde.desktop = p.desktop;
            if (direct.dpiAvailable) {
                const QString dpi = QString::number(direct.dpiX) + "×" + QString::number(direct.dpiY);
                kde.speed = "DPI " + dpi + " · KDE accel " + kde.speed;
                kde.source = "Direct HID + KDE kcminputrc";
                if (direct.pollRateAvailable) kde.accelProfile += " · poll " + QString::number(direct.pollRateHz) + " Hz";
            }
            return kde;
        }
    }

    const QString speed = runCommand("gsettings", {"get", "org.gnome.desktop.peripherals.mouse", "speed"});
    if (!speed.isEmpty() && !speed.contains("No such schema")) {
        p.source = "GNOME gsettings";
        p.speed = speed;
        p.accelProfile = runCommand("gsettings", {"get", "org.gnome.desktop.peripherals.mouse", "accel-profile"});
        p.naturalScroll = runCommand("gsettings", {"get", "org.gnome.desktop.peripherals.mouse", "natural-scroll"});
        p.leftHanded = runCommand("gsettings", {"get", "org.gnome.desktop.peripherals.mouse", "left-handed"});
        return p;
    }

    const QString xinput = runCommand("xinput", {"--list", "--short"});
    if (!xinput.isEmpty()) {
        p.source = "xinput available";
        p.speed = "See xinput properties";
        return p;
    }

    p.source = "not detected";
    p.speed = "unavailable";
    diagnostics << "Pointer speed was not readable through GNOME gsettings or xinput. On KDE/other desktops, adjust support in src/MouseInfo.cpp.";
    return p;
}

Snapshot MouseInfoCollector::collect() const {
    Snapshot s;
    s.device = collectDevice(s.diagnostics);
    s.battery = collectBattery(s.device, s.diagnostics);
    s.pointer = collectPointer(s.device, s.diagnostics);

    s.backendStatus = s.battery.available && s.battery.source.toLower().contains("hidraw")
        ? "Direct HID telemetry active"
        : "Kernel HID / fallback mode";

    if (s.device.found && s.device.name.toLower().contains("deathadder")) {
        s.diagnostics << "DeathAdder-class device detected.";
        if (s.device.productId.toLower() == "00c5") {
            s.diagnostics << "Your mouse is 1532:00c5, the DeathAdder V3 HyperSpeed wireless receiver. Direct HID is used only while this app is running.";
        }
    } else if (s.device.found) {
        s.diagnostics << "Razer device detected; exact DeathAdder V3 HyperSpeed name was not confirmed by the kernel.";
    }
    return s;
}
