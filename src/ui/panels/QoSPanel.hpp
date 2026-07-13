// src/ui/panels/QoSPanel.hpp
#pragma once
#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QMap>
#include <QString>
#include "ui/QoSRecommender.hpp"

namespace nmea::ui {

class QoSPanel : public QWidget {
    Q_OBJECT
public:
    explicit QoSPanel(QWidget* parent = nullptr);

    QoSProfile currentProfile() const;
    void setProfile(const QoSProfile& profile);

private slots:
    void onProfileSelected(int index);
    void onSaveProfile();

private:
    void rebuildCombo(const QString& select);
    void loadControls(const QoSProfile& p);

    QComboBox* profile_combo_;
    QComboBox* rel_combo_;
    QComboBox* dur_combo_;
    QSpinBox*  deadline_spin_;
    QSpinBox*  lifespan_spin_;
    QMap<QString, QoSProfile> profiles_;   // biblioteca en memoria, por nombre
    QString    current_name_;
};

}  // namespace nmea::ui
