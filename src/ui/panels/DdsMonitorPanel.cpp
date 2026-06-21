// src/ui/panels/DdsMonitorPanel.cpp
#include "ui/panels/DdsMonitorPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QDialog>
#include <QTimer>
#include <QLabel>
#include <QFont>

namespace nmea::ui {

DdsMonitorPanel::DdsMonitorPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl)
{
    auto* box    = new QGroupBox("⑥ Monitor", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* inner = new QVBoxLayout(box);

    // Fila de dominio + acciones.
    auto* ctrl_row = new QHBoxLayout;
    ctrl_row->addWidget(new QLabel("Dominio:"));
    domain_spin_ = new QSpinBox;
    domain_spin_->setRange(0, 232);
    domain_spin_->setValue(0);
    domain_spin_->setFixedWidth(60);
    ctrl_row->addWidget(domain_spin_);
    auto* scan_btn   = new QPushButton("🔍 Buscar");
    auto* sample_btn = new QPushButton("👁 Leer sample");
    auto* clear_btn  = new QPushButton("✕");
    ctrl_row->addWidget(scan_btn);
    ctrl_row->addWidget(sample_btn);
    ctrl_row->addStretch();
    ctrl_row->addWidget(clear_btn);
    inner->addLayout(ctrl_row);

    // Tabla de tópicos.
    model_ = new DdsTopicModel(this);
    table_ = new QTableView;
    table_->setModel(model_);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setSectionResizeMode(
            DdsTopicModel::Topic, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(
            DdsTopicModel::Type, QHeaderView::ResizeToContents);
    table_->verticalHeader()->hide();
    inner->addWidget(table_);

    // Estado del monitor DDS (resultado del barrido).
    diag_label_ = new QLabel("— monitor inactivo —");
    diag_label_->setWordWrap(true);
    diag_label_->setObjectName("lbl_warn");
    inner->addWidget(diag_label_);

    connect(scan_btn,   &QPushButton::clicked, this, &DdsMonitorPanel::onScanClicked);
    connect(sample_btn, &QPushButton::clicked, this, &DdsMonitorPanel::onReadSampleClicked);
    connect(clear_btn,  &QPushButton::clicked, this, [this]() { model_->clear(); });
}

void DdsMonitorPanel::onScanClicked() {
    model_->clear();
    ctrl_->scanDomain(domain_spin_->value());
}

void DdsMonitorPanel::onReadSampleClicked() {
    const QModelIndex idx = table_->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "Leer sample",
                "Selecciona un tópico de la tabla primero.");
        return;
    }
    const QString topic = model_->data(
            model_->index(idx.row(), DdsTopicModel::Topic)).toString();
    const QString type = model_->data(
            model_->index(idx.row(), DdsTopicModel::Type)).toString();

    const QString err = ctrl_->startSampleStream(topic, type);
    if (!err.isEmpty()) {
        QMessageBox::information(this, "Leer sample", err);
        return;
    }

    // Diálogo de lectura en vivo: un QTimer refresca el último dato recibido
    // mientras la ventana está abierta; al cerrarla se libera el lector.
    QDialog dlg(this);
    dlg.setWindowTitle("Sample en vivo: " + topic);
    auto* lay  = new QVBoxLayout(&dlg);
    auto* head = new QLabel("<b>" + type + "</b> &nbsp;&nbsp;🔴 en vivo");
    head->setTextFormat(Qt::RichText);
    auto* body = new QLabel("Esperando datos…");
    body->setTextFormat(Qt::PlainText);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    body->setFont(mono);
    body->setMinimumWidth(360);
    auto* close_btn = new QPushButton("Cerrar");
    lay->addWidget(head);
    lay->addWidget(body);
    lay->addWidget(close_btn);
    connect(close_btn, &QPushButton::clicked, &dlg, &QDialog::accept);

    QTimer timer;
    connect(&timer, &QTimer::timeout, &dlg, [this, body]() {
        body->setText(ctrl_->pollSampleStream());
    });
    timer.start(200);  // 5 Hz de refresco

    dlg.exec();
    timer.stop();
    ctrl_->stopSampleStream();
}

void DdsMonitorPanel::onTopicDiscovered(int, QString topicName, QString typeName,
                                        int pubCount, int subCount) {
    model_->addOrUpdate({topicName, typeName, pubCount, subCount});
}

void DdsMonitorPanel::onNetworkDiagResult(QString check, bool ok, QString detail) {
    const QString icon = ok ? "✓" : "✖";
    const QString color = ok ? "#4ade80" : "#ef4444";
    const QString line = QString("<span style='color:%1'>%2 %3</span>: %4")
                         .arg(color, icon, check, detail);
    diag_label_->setText(diag_label_->text().isEmpty()
                         ? line
                         : diag_label_->text() + "<br>" + line);
    diag_label_->setTextFormat(Qt::RichText);
}

}  // namespace nmea::ui
