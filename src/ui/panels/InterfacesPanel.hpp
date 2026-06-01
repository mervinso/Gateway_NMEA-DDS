// src/ui/panels/InterfacesPanel.hpp
#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace nmea::ui {
class GatewayController;

class InterfacesPanel : public QWidget {
    Q_OBJECT
public:
    explicit InterfacesPanel(GatewayController* ctrl, QWidget* parent = nullptr);

    QString selectedSource() const;   // "/dev/ttyUSB0" o "tcp://host:port"
    int     selectedBaud()   const;

signals:
    void connected(QString source, int baud);

private slots:
    void onConnectClicked();
    void onAutoDetect();
    void refreshPorts();

private:
    GatewayController* ctrl_;
    QComboBox*  port_combo_;
    QLineEdit*  tcp_edit_;
    QComboBox*  baud_combo_;
    QPushButton* connect_btn_;
    QPushButton* autodetect_btn_;
    QLabel*     status_label_;
};
}  // namespace nmea::ui
