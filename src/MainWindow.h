#pragma once

#include "MouseInfo.h"

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    MouseInfoCollector collector;
    QTimer timer;

    QLabel *deviceName;
    QLabel *connectionState;
    QLabel *backendState;
    QLabel *batteryValue;
    QLabel *batterySubtext;
    QProgressBar *batteryBar;
    QLabel *speedValue;
    QLabel *accelValue;
    QLabel *naturalValue;
    QLabel *handedValue;
    QLabel *idsValue;
    QLabel *pathValue;
    QLabel *hidValue;
    QTextEdit *diagnosticsBox;

    QWidget *metricBlock(const QString &label, QLabel **value, QLabel **subtext = nullptr);
    QWidget *row(const QString &label, QLabel **value);
    void applyStyle();
    void setLabel(QLabel *label, const QString &text, const QString &fallback = "—");
};
