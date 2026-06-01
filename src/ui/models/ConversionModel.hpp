// src/ui/models/ConversionModel.hpp
#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QString>

namespace nmea::ui {

struct ConversionRow {
    QString deviceId;
    int     state{0};           // 0=Stopped 1=Running 2=Error
    quint64 sentencesOk{0};
    QString categories;         // "GPS, Heading"
};

class ConversionModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Col { DeviceId=0, State, Messages, Categories, ColCount };

    explicit ConversionModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& = {}) const override;
    int      columnCount(const QModelIndex& = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

    void addOrUpdate(const ConversionRow& row);
    void remove(const QString& deviceId);
    const ConversionRow* findRow(const QString& deviceId) const;

private:
    QVector<ConversionRow> rows_;
    int indexFor(const QString& deviceId) const;
};

}  // namespace nmea::ui
