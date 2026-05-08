#include "MainWindow.h"
#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Razer Mouse Monitor");
    QApplication::setOrganizationName("Local");
    MainWindow window;
    window.show();
    return app.exec();
}
