// src/ui/models/ConversionModel.cpp
#include "ui/models/ConversionModel.hpp"

#include <QColor>

namespace nmea::ui {

ConversionModel::ConversionModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int ConversionModel::rowCount(const QModelIndex&) const { return rows_.size(); }
int ConversionModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant ConversionModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= rows_.size()) return {};
    const auto& row = rows_[idx.row()];
    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
            case DeviceId:   return row.deviceId;
            case State:      return row.state==1 ? "▶ Ejecutando"
                                  : row.state==2 ? "✖ Error" : "⏸ Parado";
            case Messages:   return QString::number(row.sentencesOk);
            case Categories: return row.categories;
        }
    }
    if (role == Qt::ForegroundRole) {
        if (idx.column()==State) {
            if (row.state==1) return QColor("#4ade80");
            if (row.state==2) return QColor("#ef4444");
            return QColor("#fbbf24");
        }
    }
    return {};
}

QVariant ConversionModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case DeviceId:   return "device_id";
        case State:      return "Estado";
        case Messages:   return "Mensajes";
        case Categories: return "Categorías";
    }
    return {};
}

int ConversionModel::indexFor(const QString& deviceId) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].deviceId == deviceId) return i;
    return -1;
}

void ConversionModel::addOrUpdate(const ConversionRow& row) {
    int i = indexFor(row.deviceId);
    if (i < 0) {
        beginInsertRows({}, rows_.size(), rows_.size());
        rows_.append(row);
        endInsertRows();
    } else {
        rows_[i] = row;
        emit dataChanged(index(i, 0), index(i, ColCount-1));
    }
}

void ConversionModel::remove(const QString& deviceId) {
    int i = indexFor(deviceId);
    if (i < 0) return;
    beginRemoveRows({}, i, i);
    rows_.remove(i);
    endRemoveRows();
}

const ConversionRow* ConversionModel::findRow(const QString& deviceId) const {
    int i = indexFor(deviceId);
    return (i >= 0) ? &rows_[i] : nullptr;
}

}  // namespace nmea::ui
