// src/ui/models/DdsTopicModel.hpp
#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QString>

namespace nmea::ui {

struct DdsTopicRow {
    QString topicName;
    QString typeName;
    int     publishers{0};
    int     subscribers{0};
};

class DdsTopicModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Col { Topic=0, Type, Publishers, Subscribers, ColCount };

    explicit DdsTopicModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& = {}) const override;
    int      columnCount(const QModelIndex& = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

    void addOrUpdate(const DdsTopicRow& row);
    void clear();

private:
    QVector<DdsTopicRow> rows_;
    int indexFor(const QString& topicName) const;
};

}  // namespace nmea::ui
