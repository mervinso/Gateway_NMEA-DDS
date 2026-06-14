// tools/test_serial.cpp
// Prueba que el Gateway lee datos seriales reales por la ruta de producción
// (GatewayController → Pipeline → SerialSource → Parser → Mapper).
//   uso: test_serial <dev> <baud>   (def: /dev/ttyUSB0 115200)
#include "ui/GatewayController.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <iostream>
#include <map>

using namespace nmea::ui;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString dev  = argc > 1 ? argv[1] : "/dev/ttyUSB0";
    const int     baud = argc > 2 ? std::atoi(argv[2]) : 115200;

    GatewayController ctrl;
    int sentences = 0;
    std::map<std::string, int> byKey;            // "talker|formatter|category" → conteo
    QStringList firstNames, firstValues;

    QObject::connect(&ctrl, &GatewayController::interfaceConnected,
                     [](QString s){ std::cout << "[connected] " << s.toStdString() << "\n"; });
    QObject::connect(&ctrl, &GatewayController::interfaceError,
                     [](QString s, QString m){ std::cout << "[error] " << s.toStdString()
                                                          << ": " << m.toStdString() << "\n"; });
    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
        [&](QString src, QString talker, QString fmt, QString cat,
            QStringList names, QStringList values, double) {
            sentences++;
            byKey[(src + " | " + talker + "|" + fmt + "|" + cat).toStdString()]++;
            if (firstNames.isEmpty()) { firstNames = names; firstValues = values; }
        });

    auto pump = [&](int ms){ QElapsedTimer t; t.start();
                             while (t.elapsed() < ms) app.processEvents(QEventLoop::AllEvents, 20); };

    std::cout << "Conectando " << dev.toStdString() << " @ " << baud << " baud...\n";
    ctrl.connectInterface(dev, baud, 0);
    pump(3000);

    std::cout << "\n--- Resultados ---\n";
    std::cout << "tramas detectadas: " << sentences << "\n";
    for (auto& [k, n] : byKey) std::cout << "  " << k << "  x" << n << "\n";
    if (!firstNames.isEmpty()) {
        std::cout << "primer registro decodificado:\n";
        for (int i = 0; i < firstNames.size(); ++i)
            std::cout << "    " << firstNames[i].toStdString() << " = "
                      << (i < firstValues.size() ? firstValues[i].toStdString() : "") << "\n";
    }
    const bool ok = sentences > 0;
    std::cout << "\n" << (ok ? "PASS ✓ el Gateway lee el serial" : "FAIL ✗") << "\n";
    return ok ? 0 : 1;
}
