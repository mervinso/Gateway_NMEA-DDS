// src/ui/models/ConversionModel.cpp
#include "ui/models/ConversionModel.hpp"

namespace nmea::ui {

ConversionModel::ConversionModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int ConversionModel::rowCount(const QModelIndex&) const { return rows_.size(); }
int ConversionModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant ConversionModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || role != Qt::DisplayRole) return {};
    const auto& r = rows_[idx.row()];
    switch (idx.column()) {
        case Talker:    return r.talker.isEmpty() ? "(prop)" : r.talker;
        case Formatter: return r.formatter;
        case DeviceId:  return r.deviceId;
        case Topic:     return r.topic;
    }
    return {};
}

QVariant ConversionModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal) return {};
    switch (section) {
        case Talker:    return "Sensor";
        case Formatter: return "Trama";
        case DeviceId:  return "device_id";
        case Topic:     return "Tópico";
    }
    return {};
}

void ConversionModel::addRow(const ConversionRow& row) {
    beginInsertRows({}, rows_.size(), rows_.size());
    rows_.push_back(row);
    endInsertRows();
}

void ConversionModel::removeAt(int row) {
    if (row < 0 || row >= rows_.size()) return;
    beginRemoveRows({}, row, row);
    rows_.remove(row);
    endRemoveRows();
}

const ConversionRow* ConversionModel::at(int row) const {
    if (row < 0 || row >= rows_.size()) return nullptr;
    return &rows_[row];
}

}  // namespace nmea::ui
