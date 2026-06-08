// src/ui/panels/QoSPanel.cpp
#include "ui/panels/QoSPanel.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>

namespace nmea::ui {

QoSPanel::QoSPanel(QWidget* parent) : QWidget(parent) {
    auto* box    = new QGroupBox("④ QoS", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* form = new QFormLayout(box);

    name_label_ = new QLabel("telemetry_fast");
    name_label_->setObjectName("lbl_ok");
    form->addRow("Perfil:", name_label_);

    rel_combo_ = new QComboBox;
    rel_combo_->addItem("BEST_EFFORT", false);
    rel_combo_->addItem("RELIABLE",    true);
    form->addRow("Reliability:", rel_combo_);

    dur_combo_ = new QComboBox;
    dur_combo_->addItem("VOLATILE",        false);
    dur_combo_->addItem("TRANSIENT_LOCAL", true);
    form->addRow("Durability:", dur_combo_);

    deadline_spin_ = new QSpinBox;
    deadline_spin_->setRange(0, 60000);
    deadline_spin_->setSuffix(" ms  (0=∞)");
    form->addRow("Deadline:", deadline_spin_);

    lifespan_spin_ = new QSpinBox;
    lifespan_spin_->setRange(0, 60000);
    lifespan_spin_->setSuffix(" ms  (0=∞)");
    form->addRow("Lifespan:", lifespan_spin_);

    form->addRow(new QLabel("✎ Editable antes de ejecutar"));
}

QoSProfile QoSPanel::currentProfile() const {
    return QoSProfile{
        name_label_->text().toStdString(),
        rel_combo_->currentData().toBool(),
        dur_combo_->currentData().toBool(),
        deadline_spin_->value(),
        lifespan_spin_->value()
    };
}

void QoSPanel::setProfile(const QoSProfile& p) {
    name_label_->setText(QString::fromStdString(p.name));
    rel_combo_->setCurrentIndex(p.reliable ? 1 : 0);
    dur_combo_->setCurrentIndex(p.transient_local ? 1 : 0);
    deadline_spin_->setValue(p.deadline_ms);
    lifespan_spin_->setValue(p.lifespan_ms);
}

}  // namespace nmea::ui
