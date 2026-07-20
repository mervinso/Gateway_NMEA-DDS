// src/ui/panels/DevicesPanel.hpp
#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace nmea { class Registry; }

namespace nmea::ui {

class DevicesPanel : public QWidget {
    Q_OBJECT
public:
    explicit DevicesPanel(const nmea::Registry* registry, QWidget* parent = nullptr);

    QString deviceId() const;                 // etiqueta editable (clave @key)
    QString selectedTalker() const   { return sel_talker_; }
    QString selectedFormatter() const { return sel_formatter_; }

public slots:
    void onSentenceDetected(QString source, QString talker, QString formatter,
                            QString category, QStringList fieldNames,
                            QStringList fieldValues, double rateHz);
    void markConverted(QString talker, QString formatter);   // gris, no seleccionable
    void markAvailable(QString talker, QString formatter);   // vuelve a disponible
    void removeInterface(QString source);   // borra las tramas de esa interfaz
    void clear();

signals:
    // Emitida al seleccionar una trama disponible. proposedId = "<cat>_<talker>".
    void tramaSelected(QString talker, QString formatter, QString category,
                       QString proposedId, double rateHz);

private slots:
    void onTreeSelectionChanged();
    void checkStale();   // marca las tramas sin datos recientes

private:
    QTreeWidgetItem* talkerItem(const QString& talker);
    QTreeWidgetItem* tramaItem(QTreeWidgetItem* talkerIt, const QString& source,
                               const QString& formatter, const QString& category);

    const nmea::Registry* registry_;
    QLineEdit*   device_id_edit_;
    QTreeWidget* tree_;
    // Índice (source|talker|formatter) → nodo de trama.
    QMap<QString, QTreeWidgetItem*> trama_items_;
    QTimer* stale_timer_;
    QString sel_talker_;
    QString sel_formatter_;
};

}  // namespace nmea::ui
