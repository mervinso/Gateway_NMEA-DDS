// src/ui/panels/DevicesPanel.cpp
#include "ui/panels/DevicesPanel.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QBrush>
#include <QSet>

#include "registry/Registry.hpp"

namespace nmea::ui {

DevicesPanel::DevicesPanel(const nmea::Registry* registry, QWidget* parent)
    : QWidget(parent), registry_(registry) {
    auto* box    = new QGroupBox("② Sensores", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* inner = new QVBoxLayout(box);

    auto* form = new QFormLayout;
    device_id_edit_ = new QLineEdit;
    device_id_edit_->setPlaceholderText("clave del sensor (se autopropone)");
    form->addRow("device_id:", device_id_edit_);
    inner->addLayout(form);

    tree_ = new QTreeWidget;
    tree_->setHeaderLabels({"Sensor / Trama / Campo", "Valor", "Unidad",
                            "Categoría", "Interfaz"});
    tree_->setAlternatingRowColors(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tree_->setMinimumHeight(260);
    tree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    inner->addWidget(tree_, 1);

    connect(tree_, &QTreeWidget::itemSelectionChanged,
            this, &DevicesPanel::onTreeSelectionChanged);
}

QString DevicesPanel::deviceId() const {
    return device_id_edit_->text().trimmed();
}

QTreeWidgetItem* DevicesPanel::talkerItem(const QString& talker) {
    const QString label = talker.isEmpty() ? "(propietario)" : talker;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
        if (tree_->topLevelItem(i)->data(0, Qt::UserRole).toString() == talker)
            return tree_->topLevelItem(i);
    auto* it = new QTreeWidgetItem(tree_, {"📡 " + label});
    it->setData(0, Qt::UserRole, talker);
    it->setExpanded(true);
    return it;
}

QTreeWidgetItem* DevicesPanel::tramaItem(QTreeWidgetItem* talkerIt,
                                         const QString& source,
                                         const QString& formatter,
                                         const QString& category) {
    const QString talker = talkerIt->data(0, Qt::UserRole).toString();
    const QString key = source + "|" + talker + "|" + formatter;
    auto found = trama_items_.find(key);
    if (found != trama_items_.end()) return found.value();
    auto* it = new QTreeWidgetItem(talkerIt, {formatter, "", "", category, source});
    it->setData(0, Qt::UserRole, talker);
    it->setData(0, Qt::UserRole + 1, formatter);
    it->setData(0, Qt::UserRole + 2, category);
    it->setData(0, Qt::UserRole + 3, source);
    it->setExpanded(true);
    trama_items_[key] = it;
    return it;
}

void DevicesPanel::onSentenceDetected(QString source, QString talker,
                                      QString formatter, QString category,
                                      QStringList fieldNames,
                                      QStringList fieldValues, double /*rateHz*/) {
    if (formatter.isEmpty()) return;  // raw: no se ofrece como trama convertible
    QTreeWidgetItem* tk = talkerItem(talker);
    QTreeWidgetItem* tr = tramaItem(tk, source, formatter, category);

    const int fieldCount = fieldNames.size();
    while (tr->childCount() < fieldCount) new QTreeWidgetItem(tr);

    const SentenceDef* def =
            registry_ ? registry_->lookup(formatter.toStdString()) : nullptr;
    for (int i = 0; i < fieldCount; ++i) {
        auto* child = tr->child(i);
        child->setText(0, fieldNames[i]);
        child->setText(1, fieldValues.value(i));
        child->setFlags(child->flags() & ~Qt::ItemIsSelectable);
        if (def) {
            for (const auto& fd : def->fields)
                if (fd.name == fieldNames[i].toStdString()) {
                    child->setText(2, QString::fromStdString(fd.unit));
                    break;
                }
        }
    }
}

void DevicesPanel::onTreeSelectionChanged() {
    auto items = tree_->selectedItems();
    sel_talker_.clear();
    sel_formatter_.clear();
    if (items.isEmpty()) return;
    QTreeWidgetItem* it = items.first();
    const QString formatter = it->data(0, Qt::UserRole + 1).toString();
    if (formatter.isEmpty()) return;  // no es un nodo de trama
    if (!(it->flags() & Qt::ItemIsSelectable)) return;  // ya convertida
    const QString talker   = it->data(0, Qt::UserRole).toString();
    const QString category = it->data(0, Qt::UserRole + 2).toString();
    sel_talker_   = talker;
    sel_formatter_ = formatter;
    const QString proposed =
            category.toLower() + "_" + (talker.isEmpty() ? "x" : talker);
    if (device_id_edit_->text().trimmed().isEmpty() ||
        device_id_edit_->property("auto").toBool()) {
        device_id_edit_->setText(proposed);
        device_id_edit_->setProperty("auto", true);
    }
    emit tramaSelected(talker, formatter, category, proposed, 0.0);
}

void DevicesPanel::markConverted(QString talker, QString formatter) {
    // La conversión es global (talker|formatter): marca la trama en todas las
    // interfaces que la traigan.
    for (QTreeWidgetItem* node : trama_items_) {
        if (node->data(0, Qt::UserRole).toString() != talker ||
            node->data(0, Qt::UserRole + 1).toString() != formatter) continue;
        node->setFlags(node->flags() & ~Qt::ItemIsSelectable);
        node->setForeground(0, QBrush(Qt::darkGray));
        node->setText(0, formatter + "  ✓ (convertida)");
    }
}

void DevicesPanel::markAvailable(QString talker, QString formatter) {
    for (QTreeWidgetItem* node : trama_items_) {
        if (node->data(0, Qt::UserRole).toString() != talker ||
            node->data(0, Qt::UserRole + 1).toString() != formatter) continue;
        node->setFlags(node->flags() | Qt::ItemIsSelectable);
        node->setForeground(0, QBrush());
        node->setText(0, formatter);
    }
}

void DevicesPanel::removeInterface(QString source) {
    QSet<QTreeWidgetItem*> talkers;
    for (auto it = trama_items_.begin(); it != trama_items_.end(); ) {
        QTreeWidgetItem* node = it.value();
        if (node->data(0, Qt::UserRole + 3).toString() == source) {
            if (sel_formatter_ == node->data(0, Qt::UserRole + 1).toString() &&
                sel_talker_    == node->data(0, Qt::UserRole).toString()) {
                sel_talker_.clear();
                sel_formatter_.clear();
            }
            if (auto* parent = node->parent()) talkers.insert(parent);
            delete node;
            it = trama_items_.erase(it);
        } else {
            ++it;
        }
    }
    // Quita los nodos de talker que quedaron sin tramas.
    for (QTreeWidgetItem* tk : talkers)
        if (tk->childCount() == 0) delete tk;
}

void DevicesPanel::clear() {
    tree_->clear();
    trama_items_.clear();
    sel_talker_.clear();
    sel_formatter_.clear();
}

}  // namespace nmea::ui
