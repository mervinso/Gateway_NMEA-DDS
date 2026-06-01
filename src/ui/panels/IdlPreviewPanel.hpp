// src/ui/panels/IdlPreviewPanel.hpp
#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>

namespace nmea::ui {
class GatewayController;
class QoSPanel;

class IdlPreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit IdlPreviewPanel(GatewayController* ctrl, QWidget* parent = nullptr);

    // Actualiza la lista de formatters detectados.
    void setFormatters(const QStringList& formatters);

signals:
    void launchRequested(QString formatter);
    void saveIdlRequested(QString formatter, QString idlText);

private slots:
    void onFormatterChanged(int index);
    void onSaveClicked();
    void onLaunchClicked();

private:
    GatewayController* ctrl_;
    QComboBox*      fmt_combo_;
    QPlainTextEdit* idl_view_;
    QPushButton*    save_btn_;
    QPushButton*    launch_btn_;

    QString buildIdl(const QString& formatter) const;
};

}  // namespace nmea::ui
