// src/ui/panels/ConversionsPanel.cpp
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>

namespace nmea::ui {

ConversionsPanel::ConversionsPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl) {
    auto* box    = new QGroupBox("⑤ Tópicos activos", this);
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
            ConversionModel::Topic, QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    inner->addWidget(table_);

    auto* del_btn = new QPushButton("🗑 Eliminar tópico");
    del_btn->setObjectName("btn_stop");
    inner->addWidget(del_btn);

    connect(del_btn, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentIndex().row();
        const ConversionRow* r = model_->at(row);
        if (!r) return;
        const QString talker = r->talker, formatter = r->formatter;
        ctrl_->removeConversion(talker, formatter);   // dispara conversionRemoved
    });
}

void ConversionsPanel::onConversionAdded(QString talker, QString formatter,
                                         QString deviceId, QString topic) {
    model_->addRow({talker, formatter, deviceId, topic});
}

void ConversionsPanel::onConversionRemoved(QString talker, QString formatter) {
    for (int i = 0; i < model_->rowCount(); ++i) {
        const ConversionRow* r = model_->at(i);
        if (r && r->talker == talker && r->formatter == formatter) {
            model_->removeAt(i);
            break;
        }
    }
    emit deleteRequested(talker, formatter);  // MainWindow → markAvailable en ②
}

}  // namespace nmea::ui
