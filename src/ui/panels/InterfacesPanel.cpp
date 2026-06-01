// src/ui/panels/InterfacesPanel.cpp
#include "ui/panels/InterfacesPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSerialPortInfo>
#include <thread>

#include "capture/BaudDetector.hpp"

namespace nmea::ui {

InterfacesPanel::InterfacesPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl)
{
    auto* box    = new QGroupBox("① Interfaces disponibles", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* form = new QFormLayout(box);

    port_combo_ = new QComboBox;
    form->addRow("Puerto serie:", port_combo_);

    tcp_edit_ = new QLineEdit;
    tcp_edit_->setPlaceholderText("192.168.1.100:10110");
    form->addRow("TCP/UDP:", tcp_edit_);

    baud_combo_ = new QComboBox;
    for (int b : {4800, 9600, 38400, 57600, 115200})
        baud_combo_->addItem(QString::number(b), b);
    baud_combo_->setCurrentIndex(0);  // 4800 por defecto

    autodetect_btn_ = new QPushButton("⚙ Auto-detect");
    auto* baud_row  = new QHBoxLayout;
    baud_row->addWidget(baud_combo_);
    baud_row->addWidget(autodetect_btn_);
    form->addRow("Baud:", baud_row);

    connect_btn_ = new QPushButton("▶ Conectar");
    connect_btn_->setObjectName("btn_connect");
    form->addRow(connect_btn_);

    status_label_ = new QLabel("Sin conexión");
    status_label_->setObjectName("lbl_warn");
    form->addRow("Estado:", status_label_);

    auto* refresh_btn = new QPushButton("↺ Refrescar puertos");
    form->addRow(refresh_btn);

    connect(connect_btn_,  &QPushButton::clicked, this, &InterfacesPanel::onConnectClicked);
    connect(autodetect_btn_, &QPushButton::clicked, this, &InterfacesPanel::onAutoDetect);
    connect(refresh_btn,   &QPushButton::clicked, this, &InterfacesPanel::refreshPorts);

    refreshPorts();
}

void InterfacesPanel::refreshPorts() {
    port_combo_->clear();
    port_combo_->addItem("— seleccionar —", "");
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        const QString label = info.portName() + " — " + info.description();
        port_combo_->addItem(label, "/dev/" + info.portName());
    }
}

QString InterfacesPanel::selectedSource() const {
    const QString tcp = tcp_edit_->text().trimmed();
    if (!tcp.isEmpty()) return "tcp://" + tcp;
    return port_combo_->currentData().toString();
}

int InterfacesPanel::selectedBaud() const {
    return baud_combo_->currentData().toInt();
}

void InterfacesPanel::onConnectClicked() {
    const QString src  = selectedSource();
    const int     baud = selectedBaud();
    if (src.isEmpty()) {
        status_label_->setText("Selecciona un puerto");
        status_label_->setObjectName("lbl_warn");
        return;
    }
    status_label_->setText("Conectando…");
    status_label_->setObjectName("lbl_warn");
    connect_btn_->setEnabled(false);
    emit connected(src, baud);
    status_label_->setText("● Conectado");
    status_label_->setObjectName("lbl_ok");
    connect_btn_->setEnabled(true);
}

void InterfacesPanel::onAutoDetect() {
    const QString src = port_combo_->currentData().toString();
    if (src.isEmpty()) return;

    autodetect_btn_->setEnabled(false);
    autodetect_btn_->setText("Detectando…");

    // Corre BaudDetector en hilo aparte (std::thread) para no bloquear la UI;
    // el resultado se reencola al hilo Qt vía QMetaObject::invokeMethod.
    const std::string path = src.toStdString();
    std::thread([this, path]() {
        const int detected = nmea::BaudDetector::detect(path, 500);
        QMetaObject::invokeMethod(this, [this, detected]() {
            if (detected > 0) {
                const int idx = baud_combo_->findData(detected);
                if (idx >= 0) baud_combo_->setCurrentIndex(idx);
                status_label_->setText(QString("Baud detectado: %1").arg(detected));
                status_label_->setObjectName("lbl_ok");
            } else {
                status_label_->setText("No se detectó baud válido");
                status_label_->setObjectName("lbl_warn");
            }
            autodetect_btn_->setEnabled(true);
            autodetect_btn_->setText("⚙ Auto-detect");
        }, Qt::QueuedConnection);
    }).detach();
}

}  // namespace nmea::ui
