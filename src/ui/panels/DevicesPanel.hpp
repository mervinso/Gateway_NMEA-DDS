// src/ui/panels/DevicesPanel.hpp
#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QMap>
#include <QString>
#include <QStringList>

namespace nmea { class Registry; }

namespace nmea::ui {

class DevicesPanel : public QWidget {
    Q_OBJECT
public:
    explicit DevicesPanel(const nmea::Registry* registry, QWidget* parent = nullptr);

    QString deviceId() const;
    QStringList detectedFormatters() const;

public slots:
    void onSentenceDetected(QString formatter, QString category,
                            QStringList fieldNames, QStringList fieldValues,
                            double rateHz);
    void clear();

signals:
    void formatterListChanged(QStringList formatters);

private:
    const nmea::Registry* registry_;
    QLineEdit*   device_id_edit_;
    QTreeWidget* tree_;
    QMap<QString, QTreeWidgetItem*> formatter_items_;
    QMap<QString, QString>          formatter_categories_;
};

}  // namespace nmea::ui
