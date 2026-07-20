// src/ui/panels/ConversionsPanel.cpp
#include "ui/panels/ConversionsPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDateTime>
#include <optional>

namespace nmea::ui {

namespace {
// Diálogo modal para editar la QoS de una conversión existente.
// Sin sentencias durante este lapso, la conversión se marca "sin datos".
constexpr qint64 kStaleAfterMs = 5000;

QString claveConv(const QString& talker, const QString& formatter) {
    return talker + '|' + formatter;
}

std::optional<QoSProfile> askQos(QWidget* parent, const QoSProfile& cur) {
    QDialog dlg(parent);
    dlg.setWindowTitle("Editar QoS");
    auto* form = new QFormLayout(&dlg);

    auto* rel = new QComboBox;
    rel->addItem("BEST_EFFORT", false);
    rel->addItem("RELIABLE",    true);
    rel->setCurrentIndex(cur.reliable ? 1 : 0);
    auto* dur = new QComboBox;
    dur->addItem("VOLATILE",        false);
    dur->addItem("TRANSIENT_LOCAL", true);
    dur->setCurrentIndex(cur.transient_local ? 1 : 0);
    auto* dl = new QSpinBox; dl->setRange(0, 60000); dl->setSuffix(" ms  (0=∞)");
    dl->setValue(cur.deadline_ms);
    auto* ls = new QSpinBox; ls->setRange(0, 60000); ls->setSuffix(" ms  (0=∞)");
    ls->setValue(cur.lifespan_ms);

    form->addRow("Reliability:", rel);
    form->addRow("Durability:",  dur);
    form->addRow("Deadline:",    dl);
    form->addRow("Lifespan:",    ls);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return std::nullopt;
    return QoSProfile{"", rel->currentData().toBool(), dur->currentData().toBool(),
                      dl->value(), ls->value()};
}
}  // namespace

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

    auto* edit_btn = new QPushButton("✎ Editar QoS");
    auto* del_btn  = new QPushButton("🗑 Eliminar conversión");
    del_btn->setObjectName("btn_stop");
    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(edit_btn);
    btn_row->addWidget(del_btn);
    inner->addLayout(btn_row);

    connect(del_btn, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentIndex().row();
        const ConversionRow* r = model_->at(row);
        if (!r) return;
        const QString talker = r->talker, formatter = r->formatter;
        ctrl_->removeConversion(talker, formatter);   // dispara conversionRemoved
    });

    connect(edit_btn, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentIndex().row();
        const ConversionRow* r = model_->at(row);
        if (!r) return;
        const QString talker = r->talker, formatter = r->formatter;
        const QoSProfile cur = ctrl_->conversionQoS(talker, formatter);
        if (auto q = askQos(this, cur))
            ctrl_->updateConversionQoS(talker, formatter, *q);  // dispara conversionQoSChanged
    });

    stale_timer_ = new QTimer(this);
    connect(stale_timer_, &QTimer::timeout, this, &ConversionsPanel::refreshEstados);
    stale_timer_->start(1000);
}

void ConversionsPanel::onConversionAdded(QString talker, QString formatter,
                                         QString deviceId, QString topic, QString qos) {
    model_->addRow({talker, formatter, deviceId, topic, qos, QString{},
                    QStringLiteral("esperando…")});
}

void ConversionsPanel::onSentenceSeen(QString talker, QString formatter) {
    last_seen_[claveConv(talker, formatter)] = QDateTime::currentMSecsSinceEpoch();
}

void ConversionsPanel::refreshEstados() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < model_->rowCount(); ++i) {
        const ConversionRow* r = model_->at(i);
        if (!r) continue;
        const auto it = last_seen_.constFind(claveConv(r->talker, r->formatter));
        if (it == last_seen_.constEnd()) continue;  // aún sin datos: "esperando…"
        const qint64 age = now - it.value();
        model_->setEstado(r->talker, r->formatter,
                          age > kStaleAfterMs
                              ? QString("⚠ sin datos (%1s)").arg(age / 1000)
                              : QStringLiteral("● recibiendo"));
    }
}

void ConversionsPanel::onConversionQoSChanged(QString talker, QString formatter,
                                              QString qos) {
    for (int i = 0; i < model_->rowCount(); ++i) {
        const ConversionRow* r = model_->at(i);
        if (r && r->talker == talker && r->formatter == formatter) {
            model_->setQos(i, qos);
            break;
        }
    }
}

void ConversionsPanel::markRos(QString talker, QString formatter, QString rosTopic) {
    model_->setRos(talker, formatter, rosTopic);
}

void ConversionsPanel::onConversionRemoved(QString talker, QString formatter) {
    for (int i = 0; i < model_->rowCount(); ++i) {
        const ConversionRow* r = model_->at(i);
        if (r && r->talker == talker && r->formatter == formatter) {
            model_->removeAt(i);
            break;
        }
    }
    last_seen_.remove(claveConv(talker, formatter));
    emit deleteRequested(talker, formatter);  // MainWindow → markAvailable en ②
}

}  // namespace nmea::ui
