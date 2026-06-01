// src/ui/models/DdsTopicModel.cpp
#include "ui/models/DdsTopicModel.hpp"

#include <QColor>

namespace nmea::ui {

DdsTopicModel::DdsTopicModel(QObject* parent) : QAbstractTableModel(parent) {}

int DdsTopicModel::rowCount(const QModelIndex&) const { return rows_.size(); }
int DdsTopicModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant DdsTopicModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= rows_.size()) return {};
    const auto& row = rows_[idx.row()];
    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
            case Topic:       return row.topicName;
            case Type:        return row.typeName;
            case Publishers:  return row.publishers;
            case Subscribers: return row.subscribers;
        }
    }
    if (role == Qt::ForegroundRole && idx.column() == Publishers)
        return row.publishers > 0 ? QColor("#4ade80") : QColor("#94a3b8");
    return {};
}

QVariant DdsTopicModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch(section) {
        case Topic:       return "Tópico";
        case Type:        return "Tipo";
        case Publishers:  return "Pub";
        case Subscribers: return "Sub";
    }
    return {};
}

int DdsTopicModel::indexFor(const QString& topicName) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].topicName == topicName) return i;
    return -1;
}

void DdsTopicModel::addOrUpdate(const DdsTopicRow& row) {
    int i = indexFor(row.topicName);
    if (i < 0) {
        beginInsertRows({}, rows_.size(), rows_.size());
        rows_.append(row);
        endInsertRows();
    } else {
        rows_[i] = row;
        emit dataChanged(index(i, 0), index(i, ColCount-1));
    }
}

void DdsTopicModel::clear() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

}  // namespace nmea::ui
