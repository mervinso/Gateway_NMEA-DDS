// src/ui/panels/ConversionsPanel.cpp
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>

namespace nmea::ui {

ConversionsPanel::ConversionsPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl)
{
    auto* box    = new QGroupBox("⑤ Topico", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* inner = new QVBoxLayout(box);

    model_ = new ConversionModel(this);
    table_ = new QTableView;
    table_->setModel(model_);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(
            ConversionModel::DeviceId, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(
            ConversionModel::State, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(
            ConversionModel::Messages, QHeaderView::ResizeToContents);
    table_->verticalHeader()->hide();
    inner->addWidget(table_);

    // Botón de acción contextual (stop seleccionado).
    auto* stop_btn = new QPushButton("⏹ Detener seleccionada");
    stop_btn->setObjectName("btn_stop");
    inner->addWidget(stop_btn);

    connect(stop_btn, &QPushButton::clicked, this, [this]() {
        const auto idx = table_->currentIndex();
        if (!idx.isValid()) return;
        const QString devId = model_->data(
                model_->index(idx.row(), ConversionModel::DeviceId)).toString();
        ctrl_->stopConversion(devId);
    });
}

void ConversionsPanel::onConversionStateChanged(
        QString deviceId, int state, quint64 sentencesOk) {
    model_->addOrUpdate({deviceId, state, sentencesOk, ""});
}

}  // namespace nmea::ui
