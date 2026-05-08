#include "MouseInfo.h"
#include "RazerHidBackend.h"
#include <QtTest/QtTest>

class MouseInfoTests : public QObject {
    Q_OBJECT
private slots:
    void parsesBluetoothRazerMouseBattery() {
        const QString line = "Device AA:BB:CC:DD:EE:FF Razer DeathAdder V3 HyperSpeed";
        const QString info = R"TXT(Device AA:BB:CC:DD:EE:FF (public)
	Name: Razer DeathAdder V3 HyperSpeed
	Alias: Razer DeathAdder V3 HyperSpeed
	Icon: input-mouse
	Paired: yes
	Connected: yes
	Modalias: usb:v1532p00BAd0001
	Battery Percentage: 0x63 (99)
)TXT";
        QStringList diagnostics;
        const DeviceInfo d = MouseInfoCollector::parseBluetoothctlDevice(line, info, &diagnostics);
        QVERIFY(d.found);
        QCOMPARE(d.name, QString("Razer DeathAdder V3 HyperSpeed"));
        QCOMPARE(d.connection, QString("Bluetooth · connected"));
        QCOMPARE(d.vendorId, QString("1532"));
        QVERIFY(d.usbDetails.join("\n").contains("Bluetooth battery: 99%"));
    }

    void parsesKdeMouseConfigBeforeGnomeFallback() {
        const QString cfg = R"CFG([Mouse]
XLbInptPointerAcceleration=-0.400
X11LibInputXAccelProfileFlat=true
MouseButtonMapping=RightHanded
)CFG";
        const PointerSettings p = MouseInfoCollector::parseKdeMouseConfig(cfg);
        QCOMPARE(p.source, QString("KDE kcminputrc"));
        QCOMPARE(p.speed, QString("-0.400"));
        QCOMPARE(p.accelProfile, QString("flat"));
        QCOMPARE(p.leftHanded, QString("false"));
    }

    void parsesInputUeventAsRazerHidName() {
        const QString uevent = R"TXT(PRODUCT=3/1532/c5/111
NAME="Razer Razer DeathAdder V3 HyperSpeed Mouse"
PHYS="usb-0000:2d:00.3-4/input1"
MODALIAS=input:b0003v1532p00C5e0111-e0,1,2,4,k110,111
)TXT";
        const DeviceInfo d = MouseInfoCollector::parseInputUevent("/sys/class/input/event4", uevent);
        QVERIFY(d.found);
        QCOMPARE(d.name, QString("Razer Razer DeathAdder V3 HyperSpeed Mouse"));
        QCOMPARE(d.vendorId, QString("1532"));
        QCOMPARE(d.productId, QString("00c5"));
        QVERIFY(d.hidNames.contains("Razer Razer DeathAdder V3 HyperSpeed Mouse"));
    }

    void parsesRazerHidrawUdevProperties() {
        const QString props = R"TXT(DEVNAME=/dev/hidraw2
ID_VENDOR_ID=1532
ID_MODEL_ID=00c5
ID_MODEL=Razer_DeathAdder_V3_HyperSpeed
)TXT";
        const RazerHidCandidate c = RazerHidBackend::parseUdevProperties(props);
        QVERIFY(c.matches);
        QCOMPARE(c.devName, QString("/dev/hidraw2"));
        QCOMPARE(c.vendorId, QString("1532"));
        QCOMPARE(c.productId, QString("00c5"));
    }

    void buildsRazerBatteryReportWithChecksum() {
        const QByteArray r = RazerHidBackend::buildReport(0x07, 0x80, 0x02);
        QCOMPARE(r.size(), 91);
        QCOMPARE(static_cast<unsigned char>(r[0]), static_cast<unsigned char>(0x00));
        QCOMPARE(static_cast<unsigned char>(r[1 + 1]), static_cast<unsigned char>(0x1f));
        QCOMPARE(static_cast<unsigned char>(r[1 + 5]), static_cast<unsigned char>(0x02));
        QCOMPARE(static_cast<unsigned char>(r[1 + 6]), static_cast<unsigned char>(0x07));
        QCOMPARE(static_cast<unsigned char>(r[1 + 7]), static_cast<unsigned char>(0x80));
        unsigned char crc = 0;
        for (int i = 2; i < 88; ++i) crc ^= static_cast<unsigned char>(r[1 + i]);
        QCOMPARE(static_cast<unsigned char>(r[1 + 88]), crc);
    }

    void parsesDirectHidTelemetryResponses() {
        QByteArray battery = RazerHidBackend::buildReport(0x07, 0x80, 0x02);
        battery[1 + 0] = 0x02;
        battery[1 + 8 + 1] = static_cast<char>(128);
        int pct = -1;
        QVERIFY(RazerHidBackend::parseBatteryResponse(battery, &pct));
        QCOMPARE(pct, 50);

        QByteArray dpi = RazerHidBackend::buildReport(0x04, 0x85, 0x07);
        dpi[1 + 0] = 0x02;
        dpi[1 + 8 + 1] = 0x06;
        dpi[1 + 8 + 2] = 0x40;
        dpi[1 + 8 + 3] = 0x06;
        dpi[1 + 8 + 4] = 0x40;
        int x = -1, y = -1;
        QVERIFY(RazerHidBackend::parseDpiResponse(dpi, &x, &y));
        QCOMPARE(x, 1600);
        QCOMPARE(y, 1600);

        QByteArray poll = RazerHidBackend::buildReport(0x00, 0x85, 0x01);
        poll[1 + 0] = 0x02;
        poll[1 + 8 + 0] = 0x01;
        int hz = -1;
        QVERIFY(RazerHidBackend::parsePollRateResponse(poll, &hz));
        QCOMPARE(hz, 1000);
    }

};

QTEST_MAIN(MouseInfoTests)
#include "test_mouseinfo.moc"
