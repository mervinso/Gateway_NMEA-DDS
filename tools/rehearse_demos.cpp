// tools/rehearse_demos.cpp
// Ensayo headless de dos flujos de riesgo, por la misma ruta que dispara la GUI:
//   Demo 3      — editar QoS en caliente sobre la conversión activa GP/GLL
//                 (updateConversionQoS recrea el writer sin parar el pipeline).
//   Demo 5 p.1  — eliminar la conversión y reconvertirla con "Publicar como ROS"
//                 (removeConversion → enableRos → addConversion).
// Verificación externa durante las ventanas: dds_dynamic_sub (post-edición QoS)
// y `ros2 topic echo /gps/fix` (post-reconversión).
//   uso: rehearse_demos <udp_port>   (def: 3102)
#include "ui/GatewayController.hpp"
#include "ros/RosPublisher.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <iostream>

using namespace nmea::ui;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString source = QString("udp://%1").arg(argc > 1 ? std::atoi(argv[1]) : 3102);

    GatewayController ctrl;
    int gll = 0, qosChanges = 0, removed = 0;
    QString lastError;

    QObject::connect(&ctrl, &GatewayController::sentenceDetected,
        [&](QString, QString tk, QString fmt, QString, QStringList, QStringList, double) {
            if (tk == "GP" && fmt == "GLL") gll++; });
    QObject::connect(&ctrl, &GatewayController::conversionAdded,
        [](QString, QString f, QString, QString topic, QString qos) {
            std::cout << "[added]   " << f.toStdString() << " -> " << topic.toStdString()
                      << "  QoS=" << qos.toStdString() << std::endl; });
    QObject::connect(&ctrl, &GatewayController::conversionQoSChanged,
        [&](QString, QString f, QString qos) {
            qosChanges++;
            std::cout << "[qos chg] " << f.toStdString() << "  QoS=" << qos.toStdString()
                      << std::endl; });
    QObject::connect(&ctrl, &GatewayController::conversionRemoved,
        [&](QString, QString f) {
            removed++;
            std::cout << "[removed] " << f.toStdString() << std::endl; });
    QObject::connect(&ctrl, &GatewayController::interfaceError,
        [&](QString, QString msg) {
            lastError = msg;
            std::cout << "[ERROR]   " << msg.toStdString() << std::endl; });

    auto pump = [&](int ms){ QElapsedTimer t; t.start();
                             while (t.elapsed() < ms) app.processEvents(QEventLoop::AllEvents, 20); };

    std::cout << "1) Conectar " << source.toStdString() << std::endl;
    ctrl.connectInterface(source, 0, 0);
    pump(2000);
    const int afterConnect = gll;

    std::cout << "2) Convertir GP/GLL con telemetry_fast (BE/VOL)" << std::endl;
    ctrl.addConversion("GP", "GLL", "gps_babor",
                       nmea::QoSProfile{"telemetry_fast", false, false, 0, 0});
    pump(3000);
    const int afterConvert = gll;

    std::cout << "3) DEMO 3: editar QoS -> RELIABLE/TRANSIENT_LOCAL en caliente" << std::endl;
    ctrl.updateConversionQoS("GP", "GLL",
                             nmea::QoSProfile{"state_latched", true, true, 0, 0});
    std::cout << "[ventana demo3] 15 s para verificacion externa (dds_dynamic_sub)"
              << std::endl;
    pump(15000);
    const int afterEdit = gll;

    std::cout << "4) DEMO 5 paso 1: eliminar conversion GP/GLL" << std::endl;
    ctrl.removeConversion("GP", "GLL");
    pump(2000);

    std::cout << "5) Reconvertir con 'Publicar como ROS' (/gps/fix, frame gps)" << std::endl;
    ctrl.enableRos("GP", "GLL",
                   nmea::ros::RosTarget{nmea::ros::RosTarget::NavSatFix, "/gps/fix", "gps"});
    ctrl.addConversion("GP", "GLL", "gps_babor",
                       nmea::QoSProfile{"telemetry_fast", false, false, 0, 0});
    std::cout << "[ventana demo5] 20 s para verificacion externa (ros2 topic echo /gps/fix)"
              << std::endl;
    pump(20000);
    const int afterReconvert = gll;

    std::cout << "\n--- Resultados ---" << std::endl;
    std::cout << "GLL tras conectar:     " << afterConnect   << std::endl;
    std::cout << "GLL tras convertir:    " << afterConvert   << std::endl;
    std::cout << "GLL tras editar QoS:   " << afterEdit      << std::endl;
    std::cout << "GLL tras reconvertir:  " << afterReconvert << std::endl;
    std::cout << "conversionQoSChanged:  " << qosChanges << std::endl;
    std::cout << "conversionRemoved:     " << removed    << std::endl;

    const bool demo3 = afterConnect > 0 && qosChanges == 1 && afterEdit > afterConvert
                       && lastError.isEmpty();
    const bool demo5 = removed == 1 && afterReconvert > afterEdit && lastError.isEmpty();
    std::cout << "\nDEMO 3 (QoS en caliente):        " << (demo3 ? "PASS" : "FAIL") << std::endl;
    std::cout << "DEMO 5 p.1 (reconvertir + ROS):  " << (demo5 ? "PASS" : "FAIL") << std::endl;
    return (demo3 && demo5) ? 0 : 1;
}
