#include "RazerHidBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QtGlobal>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int kReportIdOffset = 1;
constexpr int kRazerReportLen = 90;
constexpr int kHidrawFeatureLen = 91; // report-id byte + 90-byte Razer report
constexpr quint8 kDeathAdderV3HyperSpeedWirelessPid = 0xc5;
constexpr quint8 kRazerSuccess = 0x02;
constexpr quint8 kRazerBusy = 0x01;

QString run(const QString &program, const QStringList &args, int timeoutMs = 900) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(150)) return {};
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(150);
        return {};
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    return out.isEmpty() ? err : out;
}
}

quint8 RazerHidBackend::byteAt(const QByteArray &data, int offset) {
    if (offset < 0 || offset >= data.size()) return 0;
    return static_cast<quint8>(data.at(offset));
}

RazerHidCandidate RazerHidBackend::parseUdevProperties(const QString &properties) {
    RazerHidCandidate c;
    const QStringList lines = properties.split('\n', Qt::SkipEmptyParts);
    QHash<QString, QString> map;
    for (const QString &line : lines) {
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        map.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    c.devName = map.value("DEVNAME");
    c.vendorId = map.value("ID_VENDOR_ID").toLower();
    c.productId = map.value("ID_MODEL_ID").toLower();
    c.model = map.value("ID_MODEL").replace('_', ' ');
    c.matches = c.vendorId == "1532" && c.productId == "00c5";
    return c;
}

QByteArray RazerHidBackend::buildReport(quint8 commandClass, quint8 commandId, quint8 dataSize, quint8 transactionId, const QByteArray &arguments) {
    QByteArray buf(kHidrawFeatureLen, '\0');
    buf[0] = 0x00; // HID report ID 0 for hidraw feature ioctl.
    buf[kReportIdOffset + 0] = 0x00; // new command
    buf[kReportIdOffset + 1] = static_cast<char>(transactionId);
    buf[kReportIdOffset + 2] = 0x00; // remaining packets, big-endian high
    buf[kReportIdOffset + 3] = 0x00; // remaining packets, big-endian low
    buf[kReportIdOffset + 4] = 0x00; // protocol type
    buf[kReportIdOffset + 5] = static_cast<char>(dataSize);
    buf[kReportIdOffset + 6] = static_cast<char>(commandClass);
    buf[kReportIdOffset + 7] = static_cast<char>(commandId);
    const int argCount = qMin(arguments.size(), 80);
    for (int i = 0; i < argCount; ++i) buf[kReportIdOffset + 8 + i] = arguments.at(i);

    quint8 crc = 0;
    for (int i = 2; i < 88; ++i) crc ^= byteAt(buf, kReportIdOffset + i);
    buf[kReportIdOffset + 88] = static_cast<char>(crc);
    buf[kReportIdOffset + 89] = 0x00;
    return buf;
}

bool RazerHidBackend::basicResponseOk(const QByteArray &response, quint8 commandClass, quint8 commandId, QString *error) {
    if (response.size() < kRazerReportLen) {
        if (error) *error = "short HID response";
        return false;
    }
    const int off = response.size() >= kHidrawFeatureLen ? kReportIdOffset : 0;
    const quint8 status = byteAt(response, off + 0);
    if (status != kRazerSuccess && status != kRazerBusy) {
        if (error) *error = QString("Razer command failed/status 0x%1").arg(status, 2, 16, QLatin1Char('0'));
        return false;
    }
    if (byteAt(response, off + 6) != commandClass || byteAt(response, off + 7) != commandId) {
        if (error) *error = "response command did not match request";
        return false;
    }
    return true;
}

bool RazerHidBackend::parseBatteryResponse(const QByteArray &response, int *batteryPercent, QString *error) {
    if (!basicResponseOk(response, 0x07, 0x80, error)) return false;
    const int off = response.size() >= kHidrawFeatureLen ? kReportIdOffset : 0;
    const int raw = byteAt(response, off + 8 + 1);
    const int pct = qBound(0, qRound(raw * 100.0 / 255.0), 100);
    if (batteryPercent) *batteryPercent = pct;
    return true;
}

bool RazerHidBackend::parseChargingResponse(const QByteArray &response, bool *charging, QString *error) {
    if (!basicResponseOk(response, 0x07, 0x84, error)) return false;
    const int off = response.size() >= kHidrawFeatureLen ? kReportIdOffset : 0;
    if (charging) *charging = byteAt(response, off + 8 + 1) != 0;
    return true;
}

bool RazerHidBackend::parseDpiResponse(const QByteArray &response, int *x, int *y, QString *error) {
    if (!basicResponseOk(response, 0x04, 0x85, error)) return false;
    const int off = response.size() >= kHidrawFeatureLen ? kReportIdOffset : 0;
    const int dpiX = (byteAt(response, off + 8 + 1) << 8) | byteAt(response, off + 8 + 2);
    const int dpiY = (byteAt(response, off + 8 + 3) << 8) | byteAt(response, off + 8 + 4);
    if (dpiX <= 0 || dpiY <= 0) {
        if (error) *error = "DPI response contained zero DPI";
        return false;
    }
    if (x) *x = dpiX;
    if (y) *y = dpiY;
    return true;
}

bool RazerHidBackend::parsePollRateResponse(const QByteArray &response, int *hz, QString *error) {
    if (!basicResponseOk(response, 0x00, 0x85, error)) return false;
    const int off = response.size() >= kHidrawFeatureLen ? kReportIdOffset : 0;
    const quint8 raw = byteAt(response, off + 8 + 0);
    int value = -1;
    if (raw == 0x01) value = 1000;
    else if (raw == 0x02) value = 500;
    else if (raw == 0x08) value = 125;
    if (value < 0) {
        if (error) *error = QString("unknown poll-rate code 0x%1").arg(raw, 2, 16, QLatin1Char('0'));
        return false;
    }
    if (hz) *hz = value;
    return true;
}

QStringList RazerHidBackend::discoverHidrawPaths() {
    QDir dev("/dev");
    const QFileInfoList entries = dev.entryInfoList({"hidraw*"}, QDir::System | QDir::Files | QDir::NoSymLinks, QDir::Name);
    QStringList matches;
    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        const RazerHidCandidate c = parseUdevProperties(run("udevadm", {"info", "-q", "property", "-n", path}));
        if (c.matches) matches << path;
    }
    return matches;
}

QByteArray RazerHidBackend::sendFeatureReport(int fd, const QByteArray &request, QString *error) {
    QByteArray mutableRequest = request;
    const int sent = ioctl(fd, HIDIOCSFEATURE(kHidrawFeatureLen), mutableRequest.data());
    if (sent < 0) {
        if (error) *error = QString("HIDIOCSFEATURE failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return {};
    }
    // Razer wireless receivers need a short processing gap between the request
    // feature report and the response feature report. This is local to the
    // refresh call; no daemon/thread stays alive.
    ::usleep(6000);

    QByteArray response(kHidrawFeatureLen, '\0');
    response[0] = 0x00;
    const int got = ioctl(fd, HIDIOCGFEATURE(kHidrawFeatureLen), response.data());
    if (got < 0) {
        if (error) *error = QString("HIDIOCGFEATURE failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return {};
    }
    response.resize(got);
    return response;
}

RazerTelemetry RazerHidBackend::query() const {
    RazerTelemetry t;
    const QStringList paths = discoverHidrawPaths();
    if (paths.isEmpty()) {
        t.diagnostics << "Direct HID: no DeathAdder V3 HyperSpeed hidraw device found.";
        return t;
    }
    t.deviceFound = true;

    for (const QString &path : paths) {
        t.hidrawPath = path;
        const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM) {
                t.permissionDenied = true;
                t.diagnostics << "Direct HID: permission denied opening " + path + ". Run ./install-hidraw-permissions.sh, replug the receiver, then restart the app.";
            } else {
                t.diagnostics << QString("Direct HID: failed opening %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
            }
            continue;
        }

        t.opened = true;
        t.source = "Direct hidraw " + path;
        QString err;

        QByteArray response = sendFeatureReport(fd, buildReport(0x07, 0x80, 0x02), &err);
        if (!response.isEmpty()) {
            int pct = -1;
            QString parseErr;
            if (parseBatteryResponse(response, &pct, &parseErr)) {
                t.batteryAvailable = true;
                t.batteryPercent = pct;
            } else {
                t.diagnostics << "Direct HID battery parse: " + parseErr;
            }
        } else if (!err.isEmpty()) {
            t.diagnostics << "Direct HID battery query: " + err;
        }

        response = sendFeatureReport(fd, buildReport(0x07, 0x84, 0x02), &err);
        if (!response.isEmpty()) {
            bool charging = false;
            QString parseErr;
            if (parseChargingResponse(response, &charging, &parseErr)) {
                t.chargingAvailable = true;
                t.charging = charging;
            } else {
                t.diagnostics << "Direct HID charging parse: " + parseErr;
            }
        }

        QByteArray dpiArgs(1, '\0'); // NOSTORE
        response = sendFeatureReport(fd, buildReport(0x04, 0x85, 0x07, 0x1f, dpiArgs), &err);
        if (!response.isEmpty()) {
            int x = -1, y = -1;
            QString parseErr;
            if (parseDpiResponse(response, &x, &y, &parseErr)) {
                t.dpiAvailable = true;
                t.dpiX = x;
                t.dpiY = y;
            } else {
                t.diagnostics << "Direct HID DPI parse: " + parseErr;
            }
        }

        response = sendFeatureReport(fd, buildReport(0x00, 0x85, 0x01), &err);
        if (!response.isEmpty()) {
            int hz = -1;
            QString parseErr;
            if (parsePollRateResponse(response, &hz, &parseErr)) {
                t.pollRateAvailable = true;
                t.pollRateHz = hz;
            } else {
                t.diagnostics << "Direct HID poll-rate parse: " + parseErr;
            }
        }

        ::close(fd);
        t.diagnostics << "Direct HID: opened, queried, and closed " + path + ".";
        return t;
    }

    return t;
}
