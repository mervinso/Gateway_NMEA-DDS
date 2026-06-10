// src/ui/panels/IdlPreviewPanel.hpp
#pragma once
#include <QWidget>
#include <QLabel>
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

    // Muestra el IDL de un formatter concreto (trama seleccionada en ②).
    void showFormatter(const QString& formatter);

signals:
    void convertRequested();                  // el usuario pulsó "Convertir"
    void saveIdlRequested(QString formatter, QString idlText);

private slots:
    void onSaveClicked();
    void onConvertClicked();

private:
    GatewayController* ctrl_;
    QLabel*         trama_label_;
    QPlainTextEdit* idl_view_;
    QPushButton*    save_btn_;
    QPushButton*    convert_btn_;
    QString         formatter_;

    QString buildIdl(const QString& formatter) const;
};

}  // namespace nmea::ui
