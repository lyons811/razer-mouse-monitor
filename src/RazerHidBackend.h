#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

struct RazerTelemetry {
    bool deviceFound = false;
    bool permissionDenied = false;
    bool opened = false;
    QString hidrawPath;
    QString source;
    QStringList diagnostics;

    bool batteryAvailable = false;
    int batteryPercent = -1;
    bool chargingAvailable = false;
    bool charging = false;
    bool dpiAvailable = false;
    int dpiX = -1;
    int dpiY = -1;
    bool pollRateAvailable = false;
    int pollRateHz = -1;
};

struct RazerHidCandidate {
    bool matches = false;
    QString devName;
    QString vendorId;
    QString productId;
    QString model;
};

class RazerHidBackend {
public:
    RazerTelemetry query() const;

    static RazerHidCandidate parseUdevProperties(const QString &properties);
    static QByteArray buildReport(quint8 commandClass, quint8 commandId, quint8 dataSize, quint8 transactionId = 0x1f, const QByteArray &arguments = {});
    static bool parseBatteryResponse(const QByteArray &response, int *batteryPercent, QString *error = nullptr);
    static bool parseChargingResponse(const QByteArray &response, bool *charging, QString *error = nullptr);
    static bool parseDpiResponse(const QByteArray &response, int *x, int *y, QString *error = nullptr);
    static bool parsePollRateResponse(const QByteArray &response, int *hz, QString *error = nullptr);

private:
    static QStringList discoverHidrawPaths();
    static QByteArray sendFeatureReport(int fd, const QByteArray &request, QString *error);
    static bool basicResponseOk(const QByteArray &response, quint8 commandClass, quint8 commandId, QString *error);
    static quint8 byteAt(const QByteArray &data, int offset);
};
