// src/ui/panels/InterfacesPanel.hpp
#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>

class QVBoxLayout;

namespace nmea::ui {
class GatewayController;

class InterfacesPanel : public QWidget {
    Q_OBJECT
public:
    explicit InterfacesPanel(GatewayController* ctrl, QWidget* parent = nullptr);

signals:
    void connected(QString source, int baud);

private slots:
    void onConnectSerial();
    void onConnectNetwork();
    void onAutoDetect();
    void refreshPorts();
    void onInterfaceConnected(QString source);
    void onInterfaceDisconnected(QString source);
    void onInterfaceError(QString source, QString message);
    void onInterfaceRate(QString source, double rateHz);

private:
    int  selectedBaud() const;
    void setMessage(const QString& text, bool ok);

    GatewayController* ctrl_;
    QComboBox*  port_combo_;
    QLineEdit*  tcp_edit_;
    QComboBox*  baud_combo_;
    QPushButton* autodetect_btn_;
    QLabel*     msg_label_;
    QVBoxLayout* conn_list_layout_;

    struct Conn { QWidget* row; QLabel* label; };
    QMap<QString, Conn> rows_;
};
}  // namespace nmea::ui
