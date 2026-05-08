#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Razer Mouse Monitor");
    resize(920, 620);

    auto *root = new QWidget(this);
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(34, 28, 34, 28);
    outer->setSpacing(22);

    auto *top = new QHBoxLayout();
    auto *titleStack = new QVBoxLayout();
    auto *eyebrow = new QLabel("RAZER MOUSE MONITOR");
    eyebrow->setObjectName("eyebrow");
    deviceName = new QLabel("Scanning…");
    deviceName->setObjectName("deviceName");
    titleStack->addWidget(eyebrow);
    titleStack->addWidget(deviceName);

    auto *refreshButton = new QPushButton("Refresh");
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refresh);

    top->addLayout(titleStack, 1);
    top->addWidget(refreshButton, 0, Qt::AlignTop);
    outer->addLayout(top);

    auto *heroLine = new QFrame();
    heroLine->setObjectName("heroLine");
    heroLine->setFixedHeight(2);
    outer->addWidget(heroLine);

    auto *metrics = new QHBoxLayout();
    metrics->setSpacing(22);
    metrics->addWidget(metricBlock("Battery", &batteryValue, &batterySubtext));
    batteryBar = new QProgressBar();
    batteryBar->setRange(0, 100);
    batteryBar->setTextVisible(false);
    batteryBar->setFixedHeight(8);
    auto *batteryWrap = qobject_cast<QWidget *>(metrics->itemAt(0)->widget());
    qobject_cast<QVBoxLayout *>(batteryWrap->layout())->addWidget(batteryBar);
    metrics->addWidget(metricBlock("Pointer speed", &speedValue));
    metrics->addWidget(metricBlock("Backend", &backendState));
    outer->addLayout(metrics);

    auto *body = new QHBoxLayout();
    body->setSpacing(26);

    auto *left = new QVBoxLayout();
    left->setSpacing(12);
    left->addWidget(row("Connection", &connectionState));
    left->addWidget(row("Acceleration", &accelValue));
    left->addWidget(row("Natural scroll", &naturalValue));
    left->addWidget(row("Left handed", &handedValue));
    left->addWidget(row("USB IDs", &idsValue));
    left->addWidget(row("Kernel path", &pathValue));
    left->addWidget(row("HID names", &hidValue));
    left->addStretch(1);

    auto *right = new QVBoxLayout();
    auto *diagTitle = new QLabel("Diagnostics");
    diagTitle->setObjectName("sectionTitle");
    diagnosticsBox = new QTextEdit();
    diagnosticsBox->setReadOnly(true);
    diagnosticsBox->setMinimumHeight(250);
    right->addWidget(diagTitle);
    right->addWidget(diagnosticsBox, 1);

    body->addLayout(left, 1);
    body->addLayout(right, 1);
    outer->addLayout(body, 1);

    setCentralWidget(root);
    applyStyle();

    connect(&timer, &QTimer::timeout, this, &MainWindow::refresh);
    timer.start(15000);
    refresh();
}

QWidget *MainWindow::metricBlock(const QString &label, QLabel **value, QLabel **subtext) {
    auto *w = new QWidget();
    w->setObjectName("metric");
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(18, 16, 18, 16);
    l->setSpacing(7);
    auto *k = new QLabel(label);
    k->setObjectName("metricLabel");
    *value = new QLabel("—");
    (*value)->setObjectName("metricValue");
    l->addWidget(k);
    l->addWidget(*value);
    if (subtext) {
        *subtext = new QLabel("—");
        (*subtext)->setObjectName("metricSubtext");
        (*subtext)->setWordWrap(true);
        l->addWidget(*subtext);
    }
    return w;
}

QWidget *MainWindow::row(const QString &label, QLabel **value) {
    auto *w = new QWidget();
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 8, 0, 8);
    l->setSpacing(18);
    auto *k = new QLabel(label);
    k->setObjectName("rowLabel");
    k->setMinimumWidth(120);
    *value = new QLabel("—");
    (*value)->setObjectName("rowValue");
    (*value)->setWordWrap(true);
    (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(k, 0, Qt::AlignTop);
    l->addWidget(*value, 1);
    return w;
}

void MainWindow::setLabel(QLabel *label, const QString &text, const QString &fallback) {
    label->setText(text.trimmed().isEmpty() ? fallback : text.trimmed());
}

void MainWindow::refresh() {
    const Snapshot s = collector.collect();

    setLabel(deviceName, s.device.found ? s.device.name : "No Razer mouse detected");
    setLabel(connectionState, s.device.connection);
    setLabel(backendState, s.backendStatus);
    setLabel(speedValue, s.pointer.speed);
    setLabel(accelValue, s.pointer.accelProfile);
    setLabel(naturalValue, s.pointer.naturalScroll);
    setLabel(handedValue, s.pointer.leftHanded);
    setLabel(idsValue, s.device.vendorId.isEmpty() ? QString() : s.device.vendorId + ":" + s.device.productId);
    setLabel(pathValue, s.device.kernelPath);
    setLabel(hidValue, s.device.hidNames.join("\n"));

    if (s.battery.available) {
        setLabel(batteryValue, s.battery.capacity);
        setLabel(batterySubtext, QStringList{s.battery.status, s.battery.model, s.battery.source}.join(" · "));
        bool ok = false;
        int pct = QString(s.battery.capacity).remove('%').trimmed().toInt(&ok);
        batteryBar->setValue(ok ? pct : 0);
        batteryBar->setEnabled(ok);
    } else {
        batteryValue->setText("Unknown");
        batterySubtext->setText("Not exposed by the kernel/UPower yet");
        batteryBar->setValue(0);
        batteryBar->setEnabled(false);
    }

    QStringList lines;
    lines << "Last refresh: " + QDateTime::currentDateTime().toString(Qt::ISODate);
    lines << "Desktop: " + s.pointer.desktop;
    lines << "Pointer source: " + s.pointer.source;
    if (!s.device.usbDetails.isEmpty()) lines << s.device.usbDetails;
    lines << "";
    lines << s.diagnostics;
    diagnosticsBox->setPlainText(lines.join('\n'));
}

void MainWindow::applyStyle() {
    qApp->setStyleSheet(R"CSS(
        QMainWindow, QWidget { background: #080b09; color: #e7f0e9; font-family: Inter, Cantarell, Noto Sans, sans-serif; }
        #eyebrow { color: #44d62c; font-weight: 800; letter-spacing: 2px; font-size: 12px; }
        #deviceName { color: #f5fff6; font-size: 34px; font-weight: 800; }
        #heroLine { background: #44d62c; border: 0; }
        QPushButton { background: transparent; border: 1px solid #2a3a2d; color: #e7f0e9; padding: 10px 18px; border-radius: 18px; font-weight: 700; }
        QPushButton:hover { border-color: #44d62c; color: #44d62c; }
        #metric { background: #101511; border: 1px solid #203024; border-radius: 22px; }
        #metricLabel, #rowLabel { color: #829086; font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; }
        #metricValue { color: #ffffff; font-size: 27px; font-weight: 800; }
        #metricSubtext { color: #a8b5ab; font-size: 12px; }
        #rowValue { color: #e7f0e9; font-size: 14px; }
        #sectionTitle { color: #f5fff6; font-size: 18px; font-weight: 800; }
        QTextEdit { background: #0d120f; color: #b8c6bb; border: 1px solid #203024; border-radius: 16px; padding: 14px; font-family: JetBrains Mono, monospace; font-size: 12px; }
        QProgressBar { background: #1b241d; border: none; border-radius: 4px; }
        QProgressBar::chunk { background: #44d62c; border-radius: 4px; }
        QProgressBar:disabled::chunk { background: #3b423d; }
    )CSS");
}
