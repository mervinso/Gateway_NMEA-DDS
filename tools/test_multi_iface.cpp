// tools/test_multi_iface.cpp
// Arnés headless para verificar la lógica multi-interfaz de GatewayController
// contra el simulador NMEA en vivo (UDP 3100). No requiere GUI.
//
// Comprueba:
//   1. connectInterface abre un pipeline y emite interfaceConnected + fluyen tramas.
//   2. Reconectar la misma fuente se rechaza con interfaceError (sin reemplazar).
//   3. Una segunda fuente coexiste (varios pipelines simultáneos).
//   4. Una fuente inválida emite interfaceError ("No se pudo abrir").
//   5. disconnectInterface(source) emite interfaceDisconnected solo de esa fuente.
#include "ui/GatewayController.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <iostream>

using namespace nmea::ui;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    GatewayController ctrl;

    int connected = 0, disconnected = 0, errors = 0, sentences = 0;
    QString lastError;

    QObject::connect(&ctrl, &GatewayController::interfaceConnected, [&](QString s) {
        connected++; std::cout << "  [connected]    " << s.toStdString() << "\n";
    });
    QObject::connect(&ctrl, &GatewayController::interfaceDisconnected, [&](QString s) {
        disconnected++; std::cout << "  [disconnected] " << s.toStdString() << "\n";
    });
    QObject::connect(&ctrl, &GatewayController::interfaceError, [&](QString s, QString m) {
        errors++; lastError = m;
        std::cout << "  [error]        " << s.toStdString() << ": " << m.toStdString() << "\n";
    });
    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
                     [&](QString, QString, QString, QString, QStringList, QStringList, double) {
        sentences++;
    });

    auto pump = [&](int ms) {
        QElapsedTimer t; t.start();
        while (t.elapsed() < ms) app.processEvents(QEventLoop::AllEvents, 20);
    };

    std::cout << "1) Conectar udp://3100 (simulador en vivo)\n";
    ctrl.connectInterface("udp://3100", 0, 0);
    pump(2500);

    std::cout << "2) Reconectar udp://3100 (debe rechazarse como duplicado)\n";
    ctrl.connectInterface("udp://3100", 0, 0);
    pump(200);

    std::cout << "3) Conectar segunda fuente udp://3101 (coexiste, sin datos)\n";
    ctrl.connectInterface("udp://3101", 0, 0);
    pump(400);

    std::cout << "4) Conectar fuente inválida tcp://127.0.0.1:1 (debe fallar)\n";
    ctrl.connectInterface("tcp://127.0.0.1:1", 0, 0);
    pump(400);

    std::cout << "5) Desconectar udp://3100 (3101 debe permanecer)\n";
    ctrl.disconnectInterface("udp://3100");
    pump(300);

    std::cout << "\n--- Resultados ---\n";
    std::cout << "tramas recibidas (udp://3100): " << sentences << "\n";
    std::cout << "interfaceConnected:    " << connected    << " (esperado 2)\n";
    std::cout << "interfaceError:        " << errors       << " (esperado 2)\n";
    std::cout << "interfaceDisconnected: " << disconnected << " (esperado 1)\n";

    const bool ok =
        sentences > 0 && connected == 2 && errors == 2 && disconnected == 1;
    std::cout << "\n" << (ok ? "PASS ✓" : "FAIL ✗") << "\n";
    return ok ? 0 : 1;
}
