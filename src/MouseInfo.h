#pragma once

#include <QString>
#include <QStringList>

struct BatteryInfo {
    bool available = false;
    QString source;
    QString capacity;
    QString status;
    QString model;
};

struct DeviceInfo {
    bool found = false;
    QString name;
    QString vendorId;
    QString productId;
    QString manufacturer;
    QString connection;
    QString kernelPath;
    QStringList hidNames;
    QStringList usbDetails;
};

struct PointerSettings {
    QString desktop;
    QString speed;
    QString accelProfile;
    QString naturalScroll;
    QString leftHanded;
    QString source;
};

struct Snapshot {
    DeviceInfo device;
    BatteryInfo battery;
    PointerSettings pointer;
    QString backendStatus;
    QStringList diagnostics;
};

class MouseInfoCollector {
public:
    Snapshot collect() const;
    static DeviceInfo parseBluetoothctlDevice(const QString &deviceLine, const QString &infoText, QStringList *diagnostics = nullptr);
    static DeviceInfo parseInputUevent(const QString &eventPath, const QString &ueventText);
    static PointerSettings parseKdeMouseConfig(const QString &configText);

private:
    DeviceInfo collectDevice(QStringList &diagnostics) const;
    BatteryInfo collectBattery(const DeviceInfo &device, QStringList &diagnostics) const;
    PointerSettings collectPointer(const DeviceInfo &device, QStringList &diagnostics) const;

    static QString readFile(const QString &path);
    static QString runCommand(const QString &program, const QStringList &args, int timeoutMs = 1200);
    static QStringList listDirs(const QString &path);
    static bool textMatchesRazerMouse(const QString &text);
    static QString normalizePercent(const QString &value);
};
