// src/ui/panels/DdsMonitorPanel.cpp
#include "ui/panels/DdsMonitorPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>

namespace nmea::ui {

DdsMonitorPanel::DdsMonitorPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl)
{
    auto* box    = new QGroupBox("⑥ Monitor DDS", this);
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
    auto* scan_btn  = new QPushButton("🔍 Barrer");
    auto* diag_btn  = new QPushButton("⚕ Diagnósticos");
    auto* clear_btn = new QPushButton("✕");
    ctrl_row->addWidget(scan_btn);
    ctrl_row->addWidget(diag_btn);
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

    // Área de diagnósticos.
    diag_label_ = new QLabel("— sin diagnósticos —");
    diag_label_->setWordWrap(true);
    diag_label_->setObjectName("lbl_warn");
    inner->addWidget(diag_label_);

    connect(scan_btn,  &QPushButton::clicked, this, &DdsMonitorPanel::onScanClicked);
    connect(diag_btn,  &QPushButton::clicked, this, &DdsMonitorPanel::onDiagClicked);
    connect(clear_btn, &QPushButton::clicked, this, [this]() { model_->clear(); });
}

void DdsMonitorPanel::onScanClicked() {
    model_->clear();
    ctrl_->scanDomain(domain_spin_->value());
}

void DdsMonitorPanel::onDiagClicked() {
    diag_label_->setText("Ejecutando diagnósticos…");
    ctrl_->runNetworkDiagnostics();
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
    diag_label_->setText(diag_label_->text() == "— sin diagnósticos —"
                         ? line
                         : diag_label_->text() + "<br>" + line);
    diag_label_->setTextFormat(Qt::RichText);
}

}  // namespace nmea::ui
