// tools/test_qos.cpp
// Verifica el ciclo de edición de QoS en vivo contra el sensor serial real:
// conecta → convierte (BE/VOL) → edita QoS (RELIABLE/TL) → el pipeline recrea el
// writer sin crashear y los datos siguen fluyendo.
//   uso: test_qos <dev> <baud> <formatter>   (def: /dev/ttyUSB0 115200 VNYPR)
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

    GatewayController ctrl;
    int sentences = 0, qosChanges = 0;
    QString lastQos;

    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
        [&](QString, QString, QString, QString, QStringList, QStringList, double) { sentences++; });
    QObject::connect(&ctrl, &GatewayController::conversionAdded,
        [](QString, QString f, QString, QString topic, QString qos) {
            std::cout << "[added]   " << f.toStdString() << " -> " << topic.toStdString()
                      << "  QoS=" << qos.toStdString() << "\n"; });
    QObject::connect(&ctrl, &GatewayController::conversionQoSChanged,
        [&](QString, QString f, QString qos) {
            qosChanges++; lastQos = qos;
            std::cout << "[qos chg] " << f.toStdString() << "  QoS=" << qos.toStdString() << "\n"; });

    auto pump = [&](int ms){ QElapsedTimer t; t.start();
                             while (t.elapsed() < ms) app.processEvents(QEventLoop::AllEvents, 20); };

    std::cout << "1) Conectar " << dev.toStdString() << " @ " << baud << "\n";
    ctrl.connectInterface(dev, baud, 0);
    pump(1200);
    const int afterConnect = sentences;

    std::cout << "2) Convertir " << fmt.toStdString() << " con telemetry_fast (BE/VOL)\n";
    ctrl.addConversion("", fmt, "inertial_1", nmea::QoSProfile{"telemetry_fast", false, false, 0, 0});
    pump(1500);
    const int afterConvert = sentences;

    std::cout << "3) Editar QoS -> RELIABLE/TL (recrea el writer en vivo)\n";
    ctrl.updateConversionQoS("", fmt, nmea::QoSProfile{"state_latched", true, true, 200, 300});
    pump(1800);
    const int afterEdit = sentences;

    std::cout << "\n--- Resultados ---\n";
    std::cout << "tramas tras conectar:  " << afterConnect << "\n";
    std::cout << "tramas tras convertir: " << afterConvert << "\n";
    std::cout << "tramas tras editar:    " << afterEdit    << "\n";
    std::cout << "conversionQoSChanged:  " << qosChanges << " (ultimo: " << lastQos.toStdString() << ")\n";

    // PASS: hubo datos, la edición emitió la señal y el flujo continuó tras recrear.
    const bool ok = afterConnect > 0 && qosChanges == 1 && afterEdit > afterConvert;
    std::cout << "\n" << (ok ? "PASS ✓ QoS editable en vivo sin perder flujo"
                             : "FAIL ✗") << "\n";
    return ok ? 0 : 1;
}
