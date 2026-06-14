// src/ui/MainWindow.cpp
#include "ui/MainWindow.hpp"
#include "ui/GatewayController.hpp"
#include "ui/panels/InterfacesPanel.hpp"
#include "ui/panels/DevicesPanel.hpp"
#include "ui/panels/IdlPreviewPanel.hpp"
#include "ui/panels/QoSPanel.hpp"
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/panels/DdsMonitorPanel.hpp"
#include "ui/QoSRecommender.hpp"
#include "registry/Registry.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace nmea::ui {

namespace {
nmea::Category categoryFromName(const QString& name) {
    static const std::pair<const char*, nmea::Category> kMap[] = {
        {"GPS", nmea::Category::GPS}, {"Weather", nmea::Category::Weather},
        {"Heading", nmea::Category::Heading}, {"Radar", nmea::Category::Radar},
        {"Sounder", nmea::Category::Sounder}, {"Velocity", nmea::Category::Velocity},
        {"Attitude", nmea::Category::Attitude}, {"Inertial", nmea::Category::Inertial},
        {"Autopilot", nmea::Category::Autopilot}, {"Engine", nmea::Category::Engine},
        {"AIS", nmea::Category::AIS},
    };
    for (auto& [n, c] : kMap) if (name == n) return c;
    return nmea::Category::GPS;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("GW-NMEA-DDS");
    resize(1200, 720);

    controller_         = new GatewayController(this);
    interfaces_panel_   = new InterfacesPanel(controller_,  this);
    devices_panel_      = new DevicesPanel(&controller_->registry(), this);
    idl_panel_          = new IdlPreviewPanel(controller_,  this);
    qos_panel_          = new QoSPanel(this);
    conversions_panel_  = new ConversionsPanel(controller_, this);
    monitor_panel_      = new DdsMonitorPanel(controller_,  this);

    // Columna izquierda: paneles ①②③④ en scroll.
    auto* left_widget  = new QWidget;
    auto* left_layout  = new QVBoxLayout(left_widget);
    left_layout->setSpacing(6);
    left_layout->addWidget(interfaces_panel_);
    left_layout->addWidget(devices_panel_, 1);   // se estira
    left_layout->addWidget(idl_panel_);
    left_layout->addWidget(qos_panel_);

    auto* left_scroll = new QScrollArea;
    left_scroll->setWidgetResizable(true);
    left_scroll->setWidget(left_widget);
    left_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Columna derecha: paneles ⑤⑥.
    auto* right_widget = new QWidget;
    auto* right_layout = new QVBoxLayout(right_widget);
    right_layout->setSpacing(6);
    right_layout->addWidget(conversions_panel_, 1);
    right_layout->addWidget(monitor_panel_,     1);

    // Splitter horizontal.
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(left_scroll);
    splitter->addWidget(right_widget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({600, 600});

    setCentralWidget(splitter);
    statusBar()->showMessage("Gateway NMEA→DDS listo");

    connectPanels();

    // Garantiza que cada título de marco (QGroupBox) quepa completo dentro de su
    // borde: el estilo de ::title no reserva ancho, así que fijamos un mínimo
    // basado en el ancho del texto (+ margen para negrita y padding).
    for (QGroupBox* gb : findChildren<QGroupBox*>())
        gb->setMinimumWidth(gb->fontMetrics().horizontalAdvance(gb->title()) + 80);
}

void MainWindow::connectPanels() {
    // ① Interfaces → controller: abre la interfaz (preview + publicación selectiva).
    connect(interfaces_panel_, &InterfacesPanel::connected,
            this, [this](QString src, int baud) {
        controller_->connectInterface(src, baud, 0);
    });

    // controller → ② Sensores (datos en vivo, con talker e interfaz).
    connect(controller_, &GatewayController::sentenceDetected,
            devices_panel_, &DevicesPanel::onSentenceDetected);

    // Al desconectar una interfaz, sus tramas desaparecen del panel.
    connect(controller_, &GatewayController::interfaceDisconnected,
            devices_panel_, &DevicesPanel::removeInterface);

    // ② selección de trama → ③ IDL + ④ QoS auto.
    connect(devices_panel_, &DevicesPanel::tramaSelected, this,
            [this](QString talker, QString formatter, QString category,
                   QString /*proposedId*/, double rateHz) {
        idl_panel_->showFormatter(formatter);
        const nmea::Category cat = categoryFromName(category);
        qos_panel_->setProfile(nmea::QoSRecommender::recommend(cat, rateHz));
    });

    // ③ Convertir → controller.addConversion con datos de ②/④.
    connect(idl_panel_, &IdlPreviewPanel::convertRequested, this, [this]() {
        const QString talker    = devices_panel_->selectedTalker();
        const QString formatter = devices_panel_->selectedFormatter();
        const QString devId      = devices_panel_->deviceId();
        if (formatter.isEmpty() || devId.isEmpty()) {
            statusBar()->showMessage("Selecciona una trama y define device_id");
            return;
        }
        controller_->addConversion(talker, formatter, devId,
                                   qos_panel_->currentProfile());
        devices_panel_->markConverted(talker, formatter);
        statusBar()->showMessage("Tópico creado: " + formatter);
    });

    // controller → ⑤ tópicos activos.
    connect(controller_, &GatewayController::conversionAdded,
            conversions_panel_, &ConversionsPanel::onConversionAdded);
    connect(controller_, &GatewayController::conversionRemoved,
            conversions_panel_, &ConversionsPanel::onConversionRemoved);
    connect(controller_, &GatewayController::conversionQoSChanged,
            conversions_panel_, &ConversionsPanel::onConversionQoSChanged);

    // ⑤ eliminar → ② devuelve la trama a disponible.
    connect(conversions_panel_, &ConversionsPanel::deleteRequested,
            devices_panel_, &DevicesPanel::markAvailable);

    // controller → ⑥ DDS monitor (sin cambios).
    connect(controller_, &GatewayController::ddsTopicDiscovered,
            monitor_panel_, &DdsMonitorPanel::onTopicDiscovered);
    connect(controller_, &GatewayController::networkDiagResult,
            monitor_panel_, &DdsMonitorPanel::onNetworkDiagResult);
}

}  // namespace nmea::ui
