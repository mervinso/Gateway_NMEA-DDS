// main.cpp
#include <QApplication>
#include "ui/MainWindow.hpp"
#include "ui/style/DarkTheme.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Gateway NMEA → DDS");
    app.setApplicationVersion("1.0");
    app.setStyleSheet(nmea::ui::kDarkThemeQss);

    nmea::ui::MainWindow window;
    window.show();

    return app.exec();
}
