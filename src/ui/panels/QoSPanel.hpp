// src/ui/panels/QoSPanel.hpp
#pragma once
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include "ui/QoSRecommender.hpp"

namespace nmea::ui {

class QoSPanel : public QWidget {
    Q_OBJECT
public:
    explicit QoSPanel(QWidget* parent = nullptr);

    QoSProfile currentProfile() const;
    void setProfile(const QoSProfile& profile);

private:
    QLabel*    name_label_;
    QComboBox* rel_combo_;
    QComboBox* dur_combo_;
    QSpinBox*  deadline_spin_;
    QSpinBox*  lifespan_spin_;
};

}  // namespace nmea::ui
