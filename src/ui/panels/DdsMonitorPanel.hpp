// src/ui/panels/DdsMonitorPanel.hpp
#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QTableView>
#include <QLabel>
#include "ui/models/DdsTopicModel.hpp"

namespace nmea::ui {
class GatewayController;

class DdsMonitorPanel : public QWidget {
    Q_OBJECT
public:
    explicit DdsMonitorPanel(GatewayController* ctrl, QWidget* parent = nullptr);

public slots:
    void onTopicDiscovered(int domainId, QString topicName, QString typeName,
                           int pubCount, int subCount);
    void onNetworkDiagResult(QString check, bool ok, QString detail);

private slots:
    void onScanClicked();
    void onDiagClicked();

private:
    GatewayController* ctrl_;
    QSpinBox*      domain_spin_;
    DdsTopicModel* model_;
    QTableView*    table_;
    QLabel*        diag_label_;
};

}  // namespace nmea::ui
