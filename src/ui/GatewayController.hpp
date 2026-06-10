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
#include "pipeline/PublishPlan.hpp"
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

    // Abre la interfaz: un único pipeline (preview siempre + publicación selectiva).
    void connectInterface(const QString& source, int baud, int domainId);
    void disconnectInterface();

    // Habilita/Deshabilita la conversión de una trama de un sensor.
    void addConversion(const QString& talker, const QString& formatter,
                       const QString& deviceId, const QoSProfile& qos);
    void removeConversion(const QString& talker, const QString& formatter);

    void scanDomain(int domainId);
    void stopScan();
    void runNetworkDiagnostics();

    // Lee una muestra del tópico indicado y la devuelve formateada como texto.
    // Reconstruye el DynamicType desde el registro (tipos del gateway "Nmea<FMT>"
    // / "RawSentence"). Bloquea hasta ~3 s esperando un dato. Requiere un barrido
    // activo (participante del monitor en el dominio).
    QString readTopicSample(const QString& topicName, const QString& typeName);

    const Registry& registry() const { return registry_; }

signals:
    void sentenceDetected(QString talker, QString formatter, QString category,
                          QStringList fieldNames, QStringList fieldValues,
                          double rateHz);
    void conversionAdded(QString talker, QString formatter,
                         QString deviceId, QString topic);
    void conversionRemoved(QString talker, QString formatter);
    void conversionStateChanged(QString deviceId, int state, quint64 sentencesOk);
    void ddsTopicDiscovered(int domainId, QString topicName, QString typeName,
                            int pubCount, int subCount);
    void networkDiagResult(QString check, bool ok, QString detail);

private slots:
    void pollPipelines();

private:
    std::unique_ptr<nmea::ISource> makeSource(const QString& source, int baud);

    Registry registry_;
    nmea::PublishPlan publish_plan_;
    std::unique_ptr<nmea::Pipeline> iface_pipeline_;
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
