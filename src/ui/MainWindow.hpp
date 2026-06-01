// src/ui/MainWindow.hpp
#pragma once
#include <QMainWindow>
#include <QSplitter>
#include <QScrollArea>

namespace nmea::ui {
class GatewayController;
class InterfacesPanel;
class DevicesPanel;
class IdlPreviewPanel;
class QoSPanel;
class ConversionsPanel;
class DdsMonitorPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void connectPanels();
    void updateStatusBar(int activeCount);

    GatewayController* controller_;
    InterfacesPanel*   interfaces_panel_;
    DevicesPanel*      devices_panel_;
    IdlPreviewPanel*   idl_panel_;
    QoSPanel*          qos_panel_;
    ConversionsPanel*  conversions_panel_;
    DdsMonitorPanel*   monitor_panel_;
};

}  // namespace nmea::ui
