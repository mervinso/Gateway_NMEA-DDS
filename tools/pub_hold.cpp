// tools/pub_hold.cpp
// Publicador de larga duración: conecta el sensor real por la ruta del gateway,
// activa una conversión y publica en DDS por N segundos. Para pruebas de interop.
//   uso: pub_hold <dev> <baud> <formatter> <device_id> <secs>
#include "ui/GatewayController.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <iostream>

using namespace nmea::ui;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString dev = argc > 1 ? argv[1] : "/dev/ttyUSB0";
    const int    baud = argc > 2 ? std::atoi(argv[2]) : 115200;
    const QString fmt = argc > 3 ? argv[3] : "VNYPR";
    const QString id  = argc > 4 ? argv[4] : "inertial_1";
    const int    secs = argc > 5 ? std::atoi(argv[5]) : 20;

    GatewayController ctrl;
    int n = 0;
    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
        [&](QString, QString, QString, QString, QStringList, QStringList, double) { ++n; });

    ctrl.connectInterface(dev, baud, 0);
    ctrl.addConversion("", fmt, id, nmea::QoSProfile{"telemetry_fast", false, false, 0, 0});
    std::cout << "Publicando " << fmt.toStdString() << " (device_id=" << id.toStdString()
              << ") en DDS por " << secs << "s...\n";

    QElapsedTimer t; t.start();
    while (t.elapsed() < secs * 1000) app.processEvents(QEventLoop::AllEvents, 50);
    std::cout << "tramas procesadas: " << n << "\n";
    return 0;
}
