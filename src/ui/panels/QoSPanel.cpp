// src/ui/panels/QoSPanel.cpp
#include "ui/panels/QoSPanel.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QSignalBlocker>

namespace nmea::ui {

QoSPanel::QoSPanel(QWidget* parent) : QWidget(parent) {
    auto* box    = new QGroupBox("④ QoS", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* form = new QFormLayout(box);

    // Biblioteca sembrada con los dos perfiles recomendados.
    profiles_["telemetry_fast"] = {"telemetry_fast", false, false, 0, 0};
    profiles_["state_latched"]  = {"state_latched",  true,  true,  0, 0};
    current_name_ = "telemetry_fast";

    profile_combo_ = new QComboBox;
    form->addRow("Perfil:", profile_combo_);

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

    auto* save_btn = new QPushButton("💾 Guardar perfil");
    form->addRow(save_btn);
    form->addRow(new QLabel("✎ Se aplica al Convertir / Editar QoS"));

    connect(profile_combo_, &QComboBox::currentIndexChanged,
            this, &QoSPanel::onProfileSelected);
    connect(save_btn, &QPushButton::clicked, this, &QoSPanel::onSaveProfile);

    rebuildCombo(current_name_);
    loadControls(profiles_[current_name_]);
}

void QoSPanel::rebuildCombo(const QString& select) {
    QSignalBlocker block(profile_combo_);
    profile_combo_->clear();
    for (auto it = profiles_.constBegin(); it != profiles_.constEnd(); ++it)
        profile_combo_->addItem(it.key(), it.key());
    profile_combo_->addItem("＋ Nuevo…", "__new__");
    const int idx = profile_combo_->findData(select);
    profile_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void QoSPanel::loadControls(const QoSProfile& p) {
    rel_combo_->setCurrentIndex(p.reliable ? 1 : 0);
    dur_combo_->setCurrentIndex(p.transient_local ? 1 : 0);
    deadline_spin_->setValue(p.deadline_ms);
    lifespan_spin_->setValue(p.lifespan_ms);
}

void QoSPanel::onProfileSelected(int index) {
    const QString data = profile_combo_->itemData(index).toString();
    if (data == "__new__") {
        bool ok = false;
        const QString name = QInputDialog::getText(
                this, "Nuevo perfil QoS", "Nombre del perfil:",
                QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || name.isEmpty() || name == "__new__") {
            rebuildCombo(current_name_);   // revertir la selección
            return;
        }
        // Clona los valores actuales de los controles bajo el nombre nuevo.
        profiles_[name] = QoSProfile{name.toStdString(),
                rel_combo_->currentData().toBool(),
                dur_combo_->currentData().toBool(),
                deadline_spin_->value(), lifespan_spin_->value()};
        current_name_ = name;
        rebuildCombo(name);
        return;
    }
    current_name_ = data;
    loadControls(profiles_[data]);
}

void QoSPanel::onSaveProfile() {
    if (current_name_.isEmpty()) return;
    profiles_[current_name_] = QoSProfile{current_name_.toStdString(),
            rel_combo_->currentData().toBool(),
            dur_combo_->currentData().toBool(),
            deadline_spin_->value(), lifespan_spin_->value()};
}

QoSProfile QoSPanel::currentProfile() const {
    return QoSProfile{
        current_name_.toStdString(),
        rel_combo_->currentData().toBool(),
        dur_combo_->currentData().toBool(),
        deadline_spin_->value(),
        lifespan_spin_->value()
    };
}

void QoSPanel::setProfile(const QoSProfile& p) {
    const QString name = QString::fromStdString(p.name);
    if (!name.isEmpty()) {
        if (!profiles_.contains(name)) profiles_[name] = p;
        current_name_ = name;
        rebuildCombo(current_name_);
    }
    loadControls(p);
}

}  // namespace nmea::ui
