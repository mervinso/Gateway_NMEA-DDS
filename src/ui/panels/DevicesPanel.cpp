// src/ui/panels/DevicesPanel.cpp
#include "ui/panels/DevicesPanel.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHeaderView>

#include "registry/Registry.hpp"

namespace nmea::ui {

DevicesPanel::DevicesPanel(const nmea::Registry* registry, QWidget* parent)
    : QWidget(parent), registry_(registry) {
    auto* box    = new QGroupBox("② Dispositivo", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* inner = new QVBoxLayout(box);

    auto* form = new QFormLayout;
    device_id_edit_ = new QLineEdit;
    device_id_edit_->setPlaceholderText("ej: gps_proa");
    form->addRow("device_id:", device_id_edit_);
    inner->addLayout(form);

    tree_ = new QTreeWidget;
    tree_->setHeaderLabels({"Campo / Categoría", "Valor", "Unidad"});
    tree_->setAlternatingRowColors(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Altura generosa: este árbol (categorías → sentencias → campos) es la vista
    // clave para verificar los tipos de dispositivo detectados.
    tree_->setMinimumHeight(260);
    tree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    inner->addWidget(tree_, 1);
}

QString DevicesPanel::deviceId() const {
    return device_id_edit_->text().trimmed();
}

QStringList DevicesPanel::detectedFormatters() const {
    return QStringList(formatter_items_.keyBegin(), formatter_items_.keyEnd());
}

void DevicesPanel::onSentenceDetected(QString formatter, QString category,
                                      QStringList fieldNames, QStringList fieldValues,
                                      double /*rateHz*/) {
    // Crear o recuperar ítem de categoría.
    const QString catKey = category;
    QTreeWidgetItem* catItem = nullptr;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        if (tree_->topLevelItem(i)->text(0) == catKey) {
            catItem = tree_->topLevelItem(i); break;
        }
    }
    if (!catItem) {
        catItem = new QTreeWidgetItem(tree_, {catKey});
        catItem->setExpanded(true);
    }

    // Crear o recuperar ítem de formatter.
    if (!formatter_items_.contains(formatter)) {
        auto* fmtItem = new QTreeWidgetItem(catItem, {formatter});
        fmtItem->setExpanded(true);
        formatter_items_[formatter] = fmtItem;
        formatter_categories_[formatter] = category;
        emit formatterListChanged(detectedFormatters());
    }
    QTreeWidgetItem* fmtItem = formatter_items_[formatter];

    // Actualizar campos: crear si no existen, actualizar si ya están.
    // Los ítems de campo se crean como hijos de fmtItem.
    const int fieldCount = fieldNames.size();
    while (fmtItem->childCount() < fieldCount) {
        new QTreeWidgetItem(fmtItem);
    }
    // Unidad de cada campo desde el registro (fuente de verdad de unidades).
    const SentenceDef* def =
            registry_ ? registry_->lookup(formatter.toStdString()) : nullptr;
    for (int i = 0; i < fieldCount; ++i) {
        auto* child = fmtItem->child(i);
        child->setText(0, fieldNames[i]);
        child->setText(1, fieldValues.value(i));
        if (def) {
            for (const auto& fd : def->fields) {
                if (fd.name == fieldNames[i].toStdString()) {
                    child->setText(2, QString::fromStdString(fd.unit));
                    break;
                }
            }
        }
    }
}

void DevicesPanel::clear() {
    tree_->clear();
    formatter_items_.clear();
    formatter_categories_.clear();
}

}  // namespace nmea::ui
