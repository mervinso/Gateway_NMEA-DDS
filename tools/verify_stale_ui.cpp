// tools/verify_stale_ui.cpp
// Verificación offscreen del indicador de recepción en la capa de widgets:
// alimenta GP/GLL por UDP, comprueba "● recibiendo" en ⑤ y valores en ②;
// corta el flujo y comprueba que ambos paneles pasan a "⚠ sin datos" sin
// perder la conexión (la fuente sigue abierta). Correr con -platform offscreen.
//   uso: verify_stale_ui [udp_port=3103]
#include "ui/GatewayController.hpp"
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/panels/DevicesPanel.hpp"
#include "ui/models/ConversionModel.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTableView>
#include <QTreeWidget>
#include <QUdpSocket>
#include <iostream>

using namespace nmea::ui;

namespace {
QByteArray nmeaSentence(const QByteArray& body) {
    quint8 cs = 0;
    for (char c : body) cs ^= static_cast<quint8>(c);
    return "$" + body + "*" + QByteArray::number(cs, 16).rightJustified(2, '0').toUpper()
           + "\r\n";
}
}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const quint16 port = argc > 1 ? static_cast<quint16>(std::atoi(argv[1])) : 3103;

    GatewayController ctrl;
    DevicesPanel      devices(&ctrl.registry());
    ConversionsPanel  conversions(&ctrl);

    // Mismo cableado que MainWindow::connectPanels.
    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
                     &devices, &DevicesPanel::onSentenceDetected);
    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
            &conversions, [&](QString, QString tk, QString fmt, QString,
                              QStringList, QStringList, double) {
        conversions.onSentenceSeen(tk, fmt);
    });
    QObject::connect(&ctrl, &GatewayController::conversionAdded,
                     &conversions, &ConversionsPanel::onConversionAdded);

    QUdpSocket sender;
    const QByteArray gll =
            nmeaSentence("GPGLL,0816.249979,N,07932.749980,W,123456.00,A,A");

    auto pump = [&](int ms, bool feed) {
        QElapsedTimer t; t.start();
        qint64 lastSend = -500;
        while (t.elapsed() < ms) {
            if (feed && t.elapsed() - lastSend >= 250) {
                sender.writeDatagram(gll, QHostAddress::LocalHost, port);
                lastSend = t.elapsed();
            }
            app.processEvents(QEventLoop::AllEvents, 20);
        }
    };

    ctrl.connectInterface(QString("udp://%1").arg(port), 0, 0);
    ctrl.addConversion("GP", "GLL", "gps_babor",
                       nmea::QoSProfile{"telemetry_fast", false, false, 0, 0});

    std::cout << "1) Alimentando GLL 4 Hz durante 3 s…" << std::endl;
    pump(3000, true);

    auto* table = conversions.findChild<QTableView*>();
    auto* tree  = devices.findChild<QTreeWidget*>();
    auto estado = [&]() {
        return table->model()->index(0, ConversionModel::Estado).data().toString();
    };
    auto tramaAviso = [&]() -> QString {
        if (tree->topLevelItemCount() == 0 || !tree->topLevelItem(0)->childCount())
            return "(sin nodo de trama)";
        return tree->topLevelItem(0)->child(0)->text(1);
    };

    const QString e1 = estado(), a1 = tramaAviso();
    std::cout << "   Estado ⑤: '" << e1.toStdString() << "'   Valor trama ②: '"
              << a1.toStdString() << "'" << std::endl;

    std::cout << "2) Flujo cortado; esperando 8 s…" << std::endl;
    pump(8000, false);

    const QString e2 = estado(), a2 = tramaAviso();
    std::cout << "   Estado ⑤: '" << e2.toStdString() << "'   Valor trama ②: '"
              << a2.toStdString() << "'" << std::endl;

    std::cout << "3) Flujo restablecido 3 s (debe recuperarse)…" << std::endl;
    pump(3000, true);

    const QString e3 = estado(), a3 = tramaAviso();
    std::cout << "   Estado ⑤: '" << e3.toStdString() << "'   Valor trama ②: '"
              << a3.toStdString() << "'" << std::endl;

    const bool ok = e1 == "● recibiendo" && a1.isEmpty()
                 && e2.startsWith("⚠ sin datos") && a2.startsWith("⚠ sin datos")
                 && e3 == "● recibiendo" && a3.isEmpty();
    std::cout << "\n" << (ok ? "PASS ✓ recepción y ausencia de datos diferenciadas"
                             : "FAIL ✗") << std::endl;
    return ok ? 0 : 1;
}
