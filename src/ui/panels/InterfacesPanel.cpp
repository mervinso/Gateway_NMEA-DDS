// src/ui/panels/InterfacesPanel.cpp
#include "ui/panels/InterfacesPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QPointer>
#include <QSerialPortInfo>
#include <QStyle>
#include <thread>

#include "capture/BaudDetector.hpp"

namespace nmea::ui {

namespace {
// Cambiar el objectName no re-aplica el stylesheet por sí solo: forzar repolish.
void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}
}  // namespace

InterfacesPanel::InterfacesPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl)
{
    auto* box    = new QGroupBox("① Interfaces", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* outer = new QVBoxLayout(box);

    // ── Sección Serie (puerto + baud + auto-detect, con su propio Conectar) ──
    auto* serial_group = new QGroupBox("Serial");
    auto* serial_form  = new QFormLayout(serial_group);

    port_combo_ = new QComboBox;
    auto* refresh_btn = new QPushButton("↺ Refrescar");
    auto* port_row = new QHBoxLayout;
    port_row->addWidget(port_combo_, 1);
    port_row->addWidget(refresh_btn);
    serial_form->addRow("Puerto:", port_row);

    baud_combo_ = new QComboBox;
    for (int b : {4800, 9600, 38400, 57600, 115200})
        baud_combo_->addItem(QString::number(b), b);
    baud_combo_->setCurrentIndex(0);  // 4800 por defecto
    autodetect_btn_ = new QPushButton("⚙ Auto-detect");
    auto* baud_row  = new QHBoxLayout;
    baud_row->addWidget(baud_combo_, 1);
    baud_row->addWidget(autodetect_btn_);
    serial_form->addRow("Baud:", baud_row);

    auto* connect_serial_btn = new QPushButton("▶ Conectar");
    serial_form->addRow(connect_serial_btn);
    outer->addWidget(serial_group);

    // ── Sección Red TCP/UDP (con su propio Conectar) ────────────────────────
    auto* net_group = new QGroupBox("Red (TCP/UDP)");
    auto* net_form  = new QFormLayout(net_group);
    tcp_edit_ = new QLineEdit;
    tcp_edit_->setPlaceholderText("host:puerto (TCP) o udp://3100");
    net_form->addRow("Dirección:", tcp_edit_);
    auto* connect_net_btn = new QPushButton("▶ Conectar");
    net_form->addRow(connect_net_btn);
    outer->addWidget(net_group);

    msg_label_ = new QLabel;
    msg_label_->setObjectName("lbl_warn");
    msg_label_->setWordWrap(true);
    outer->addWidget(msg_label_);

    // Lista de conexiones activas: una fila por interfaz con su botón Desconectar.
    outer->addWidget(new QLabel("Conexiones activas:"));
    auto* conn_container = new QWidget;
    conn_list_layout_ = new QVBoxLayout(conn_container);
    conn_list_layout_->setContentsMargins(0,0,0,0);
    conn_list_layout_->setSpacing(2);
    outer->addWidget(conn_container);

    connect(connect_serial_btn, &QPushButton::clicked, this, &InterfacesPanel::onConnectSerial);
    connect(connect_net_btn,    &QPushButton::clicked, this, &InterfacesPanel::onConnectNetwork);
    connect(autodetect_btn_,    &QPushButton::clicked, this, &InterfacesPanel::onAutoDetect);
    connect(refresh_btn,        &QPushButton::clicked, this, &InterfacesPanel::refreshPorts);

    connect(ctrl_, &GatewayController::interfaceConnected,
            this, &InterfacesPanel::onInterfaceConnected);
    connect(ctrl_, &GatewayController::interfaceDisconnected,
            this, &InterfacesPanel::onInterfaceDisconnected);
    connect(ctrl_, &GatewayController::interfaceError,
            this, &InterfacesPanel::onInterfaceError);
    connect(ctrl_, &GatewayController::interfaceRate,
            this, &InterfacesPanel::onInterfaceRate);

    refreshPorts();
}

void InterfacesPanel::setMessage(const QString& text, bool ok) {
    msg_label_->setText(text);
    msg_label_->setObjectName(ok ? "lbl_ok" : "lbl_warn");
    repolish(msg_label_);
}

void InterfacesPanel::refreshPorts() {
    port_combo_->clear();
    port_combo_->addItem("— seleccionar —", "");
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        const QString label = info.portName() + " — " + info.description();
        port_combo_->addItem(label, "/dev/" + info.portName());
    }
}

int InterfacesPanel::selectedBaud() const {
    return baud_combo_->currentData().toInt();
}

void InterfacesPanel::onConnectSerial() {
    const QString src = port_combo_->currentData().toString();
    if (src.isEmpty()) { setMessage("Selecciona un puerto serie", false); return; }
    setMessage("Conectando " + src + "…", false);
    // El estado final (fila verde o error) lo reportan las señales del controller.
    emit connected(src, selectedBaud());
}

void InterfacesPanel::onConnectNetwork() {
    QString net = tcp_edit_->text().trimmed();
    if (net.isEmpty()) {
        setMessage("Escribe host:puerto o udp://<puerto>", false);
        return;
    }
    // Respeta un esquema explícito (udp://3100, tcp://host:port);
    // si no hay esquema, se asume TCP por compatibilidad.
    if (!net.startsWith("tcp://") && !net.startsWith("udp://")) net = "tcp://" + net;
    setMessage("Conectando " + net + "…", false);
    emit connected(net, 0);
}

void InterfacesPanel::onAutoDetect() {
    const QString src = port_combo_->currentData().toString();
    if (src.isEmpty()) { setMessage("Selecciona un puerto serie", false); return; }

    autodetect_btn_->setEnabled(false);
    autodetect_btn_->setText("Detectando…");

    // Corre BaudDetector en hilo aparte (std::thread) para no bloquear la UI;
    // el resultado se reencola al hilo Qt vía QMetaObject::invokeMethod.
    const std::string path = src.toStdString();
    QPointer<InterfacesPanel> guard(this);
    std::thread([guard, path]() {
        const int detected = nmea::BaudDetector::detect(path, 500);
        QMetaObject::invokeMethod(qApp, [guard, detected]() {
            if (!guard) return;  // panel destruido mientras detectaba
            if (detected > 0) {
                const int idx = guard->baud_combo_->findData(detected);
                if (idx >= 0) guard->baud_combo_->setCurrentIndex(idx);
                guard->setMessage(QString("Baud detectado: %1").arg(detected), true);
            } else {
                guard->setMessage("No se detectó baud válido", false);
            }
            guard->autodetect_btn_->setEnabled(true);
            guard->autodetect_btn_->setText("⚙ Auto-detect");
        }, Qt::QueuedConnection);
    }).detach();
}

void InterfacesPanel::onInterfaceConnected(QString source) {
    setMessage("Conectado: " + source, true);
    if (rows_.contains(source)) return;

    auto* row = new QWidget;
    auto* h   = new QHBoxLayout(row);
    h->setContentsMargins(0,0,0,0);
    auto* lbl = new QLabel("● " + source);
    lbl->setObjectName("lbl_warn");  // ámbar hasta que lleguen datos
    auto* btn = new QPushButton("✕ Desconectar");
    h->addWidget(lbl, 1);
    h->addWidget(btn);
    connect(btn, &QPushButton::clicked, this,
            [this, source]() { ctrl_->disconnectInterface(source); });

    conn_list_layout_->addWidget(row);
    rows_.insert(source, {row, lbl});
}

void InterfacesPanel::onInterfaceRate(QString source, double rateHz) {
    auto it = rows_.find(source);
    if (it == rows_.end()) return;
    QLabel* lbl = it->label;
    lbl->setText(QString("● %1  (%2 Hz)").arg(source).arg(rateHz, 0, 'f', 0));
    lbl->setObjectName(rateHz > 0 ? "lbl_ok" : "lbl_warn");
    repolish(lbl);
}

void InterfacesPanel::onInterfaceDisconnected(QString source) {
    auto it = rows_.find(source);
    if (it != rows_.end()) {
        it->row->deleteLater();
        rows_.erase(it);
    }
    setMessage(rows_.isEmpty() ? "Sin conexión" : "Desconectado: " + source, false);
}

void InterfacesPanel::onInterfaceError(QString /*source*/, QString message) {
    setMessage("✕ " + message, false);
}

}  // namespace nmea::ui
