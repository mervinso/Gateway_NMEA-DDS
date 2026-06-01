// src/ui/panels/ConversionsPanel.hpp
#pragma once
#include <QWidget>
#include <QTableView>
#include "ui/models/ConversionModel.hpp"

namespace nmea::ui {
class GatewayController;

class ConversionsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConversionsPanel(GatewayController* ctrl, QWidget* parent = nullptr);

public slots:
    void onConversionStateChanged(QString deviceId, int state, quint64 sentencesOk);

private:
    GatewayController* ctrl_;
    ConversionModel*   model_;
    QTableView*        table_;
};

}  // namespace nmea::ui
