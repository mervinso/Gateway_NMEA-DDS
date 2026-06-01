// src/ui/GatewayController.hpp
#pragma once
#include <map>
#include <memory>
#include <string>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"
#include "ui/QoSRecommender.hpp"

namespace eprosima::fastdds::dds { class DomainParticipant; }

namespace nmea::ui {

class MonitorListener;  // definido en el .cpp

class GatewayController : public QObject {
    Q_OBJECT
public:
    explicit GatewayController(QObject* parent = nullptr);
    ~GatewayController() override;

    void startPreview(const QString& source, int baud, const QString& deviceId);
    void stopPreview();
    void launchConversion(const QString& source, int baud,
                          const QString& deviceId, int domainId,
                          const QoSProfile& qos);
    void stopConversion(const QString& deviceId);
    void scanDomain(int domainId);
    void stopScan();
    void runNetworkDiagnostics();

    const Registry& registry() const { return registry_; }

signals:
    void sentenceDetected(QString formatter, QString category,
                          QStringList fieldNames, QStringList fieldValues,
                          double rateHz);
    void conversionStateChanged(QString deviceId, int state, quint64 sentencesOk);
    void ddsTopicDiscovered(int domainId, QString topicName, QString typeName,
                            int pubCount, int subCount);
    void networkDiagResult(QString check, bool ok, QString detail);

private slots:
    void pollPipelines();

private:
    std::unique_ptr<nmea::ISource> makeSource(const QString& source, int baud);

    Registry registry_;
    std::unique_ptr<nmea::Pipeline> preview_pipeline_;
    std::map<std::string, std::unique_ptr<nmea::Pipeline>> pipelines_;
    QTimer* poll_timer_;

    struct RateTracker { quint64 last_count{0}; quint64 prev_count{0}; double rate_hz{0.0}; };
    std::map<std::string, RateTracker> rate_trackers_;

    // Monitor DDS: participante de solo-descubrimiento por dominio.
    eprosima::fastdds::dds::DomainParticipant* monitor_participant_{nullptr};
    std::unique_ptr<MonitorListener> monitor_listener_;
    struct TopicAgg { std::string typeName; int pub{0}; int sub{0}; };
    std::map<std::string, TopicAgg> topic_agg_;
    int monitor_domain_{0};
};

}  // namespace nmea::ui
