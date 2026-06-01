// src/ui/MainWindow.cpp
#include "ui/MainWindow.hpp"
#include "ui/GatewayController.hpp"
#include "ui/panels/InterfacesPanel.hpp"
#include "ui/panels/DevicesPanel.hpp"
#include "ui/panels/IdlPreviewPanel.hpp"
#include "ui/panels/QoSPanel.hpp"
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/panels/DdsMonitorPanel.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace nmea::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Gateway NMEA 0183 → DDS  v1.0");
    resize(1200, 720);

    controller_         = new GatewayController(this);
    interfaces_panel_   = new InterfacesPanel(controller_,  this);
    devices_panel_      = new DevicesPanel(this);
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
}

void MainWindow::connectPanels() {
    // Interfaces → controller (preview) → devices.
    connect(interfaces_panel_, &InterfacesPanel::connected,
            this, [this](QString src, int baud) {
        devices_panel_->clear();
        controller_->startPreview(src, baud, devices_panel_->deviceId());
    });

    // Controller → devices panel (datos en vivo).
    connect(controller_, &GatewayController::sentenceDetected,
            devices_panel_, &DevicesPanel::onSentenceDetected);

    // Devices → IDL panel (lista de formatters).
    connect(devices_panel_, &DevicesPanel::formatterListChanged,
            idl_panel_, &IdlPreviewPanel::setFormatters);

    // IDL panel → controller (lanzar conversión).
    connect(idl_panel_, &IdlPreviewPanel::launchRequested,
            this, [this](QString /*formatter*/) {
        const QString src  = interfaces_panel_->selectedSource();
        const int     baud = interfaces_panel_->selectedBaud();
        const QString id   = devices_panel_->deviceId();
        if (src.isEmpty() || id.isEmpty()) return;
        const QoSProfile qos = qos_panel_->currentProfile();
        controller_->launchConversion(src, baud, id, 0, qos);
        statusBar()->showMessage("Conversión lanzada: " + id);
    });

    // Controller → conversions panel.
    connect(controller_, &GatewayController::conversionStateChanged,
            conversions_panel_, &ConversionsPanel::onConversionStateChanged);

    // Controller → DDS monitor.
    connect(controller_, &GatewayController::ddsTopicDiscovered,
            monitor_panel_, &DdsMonitorPanel::onTopicDiscovered);
    connect(controller_, &GatewayController::networkDiagResult,
            monitor_panel_, &DdsMonitorPanel::onNetworkDiagResult);
}

}  // namespace nmea::ui
