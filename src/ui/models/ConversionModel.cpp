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
        case Qos:       return r.qos;
        case Ros:       return r.ros.isEmpty() ? "—" : r.ros;
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
        case Qos:       return "QoS";
        case Ros:       return "ROS";
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

void ConversionModel::setQos(int row, const QString& qos) {
    if (row < 0 || row >= rows_.size()) return;
    rows_[row].qos = qos;
    emit dataChanged(index(row, Qos), index(row, Qos));
}

void ConversionModel::setRos(const QString& talker, const QString& formatter,
                             const QString& ros) {
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_[i].talker == talker && rows_[i].formatter == formatter) {
            rows_[i].ros = ros;
            emit dataChanged(index(i, Ros), index(i, Ros));
            return;
        }
    }
}

const ConversionRow* ConversionModel::at(int row) const {
    if (row < 0 || row >= rows_.size()) return nullptr;
    return &rows_[row];
}

}  // namespace nmea::ui
