// src/ui/panels/ConversionsPanel.hpp
#pragma once
#include <QWidget>
#include <QTableView>
#include <QHash>
#include <QTimer>
#include "ui/models/ConversionModel.hpp"

namespace nmea::ui {
class GatewayController;

class ConversionsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConversionsPanel(GatewayController* ctrl, QWidget* parent = nullptr);

public slots:
    void onConversionAdded(QString talker, QString formatter,
                           QString deviceId, QString topic, QString qos);
    void onConversionRemoved(QString talker, QString formatter);
    void onConversionQoSChanged(QString talker, QString formatter, QString qos);
    void markRos(QString talker, QString formatter, QString rosTopic);
    // Marca de recepción: diferencia "conversión activa" de "recibiendo datos".
    void onSentenceSeen(QString talker, QString formatter);

signals:
    // Reenvía al MainWindow para devolver la trama a "disponible" en ②.
    void deleteRequested(QString talker, QString formatter);

private:
    void refreshEstados();

    GatewayController* ctrl_;
    ConversionModel*   model_;
    QTableView*        table_;
    QHash<QString, qint64> last_seen_;   // "talker|formatter" → msecs epoch
    QTimer*            stale_timer_;
};

}  // namespace nmea::ui
