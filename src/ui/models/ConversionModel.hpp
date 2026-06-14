// src/ui/models/ConversionModel.hpp
#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QString>

namespace nmea::ui {

struct ConversionRow {
    QString talker;
    QString formatter;
    QString deviceId;
    QString topic;
    QString qos;
};

class ConversionModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Col { Talker=0, Formatter, DeviceId, Topic, Qos, ColCount };

    explicit ConversionModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& = {}) const override;
    int      columnCount(const QModelIndex& = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

    void addRow(const ConversionRow& row);
    void removeAt(int row);
    void setQos(int row, const QString& qos);
    const ConversionRow* at(int row) const;

private:
    QVector<ConversionRow> rows_;
};

}  // namespace nmea::ui
