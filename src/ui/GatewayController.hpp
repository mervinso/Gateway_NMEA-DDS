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
namespace nmea::ros { class RosPublisher; struct RosTarget; }

namespace nmea::ui {

class MonitorListener;  // definido en el .cpp

class GatewayController : public QObject {
    Q_OBJECT
public:
    explicit GatewayController(QObject* parent = nullptr);
    ~GatewayController() override;

    // Abre una interfaz (preview siempre + publicación selectiva). Soporta
    // varias fuentes simultáneas: cada `source` mantiene su propio pipeline.
    void connectInterface(const QString& source, int baud, int domainId);
    void disconnectInterface(const QString& source);
    void disconnectAll();

    // Habilita/Deshabilita la conversión de una trama de un sensor.
    void addConversion(const QString& talker, const QString& formatter,
                       const QString& deviceId, const QoSProfile& qos);
    void removeConversion(const QString& talker, const QString& formatter);

    // QoS de una conversión existente: lectura (para prellenar el editor) y
    // actualización (recrea el writer con la nueva QoS).
    QoSProfile conversionQoS(const QString& talker, const QString& formatter) const;
    void updateConversionQoS(const QString& talker, const QString& formatter,
                             const QoSProfile& qos);

    void scanDomain(int domainId);
    void stopScan();

    // Habilita/Deshabilita la publicación ROS (Imu/NavSatFix) de una conversión.
    void enableRos(const QString& talker, const QString& formatter,
                   const nmea::ros::RosTarget& target);
    void disableRos(const QString& talker, const QString& formatter);

    // Lectura en vivo de un tópico: abre un lector persistente (startSampleStream),
    // entrega la última muestra formateada en cada sondeo (pollSampleStream) y lo
    // cierra al terminar (stopSampleStream). Reconstruye el DynamicType desde el
    // registro (tipos del gateway "Nmea<FMT>" / "RawSentence"). Requiere un barrido
    // activo (participante del monitor en el dominio). startSampleStream devuelve
    // "" si abrió bien, o un mensaje de error.
    QString startSampleStream(const QString& topicName, const QString& typeName);
    QString pollSampleStream();
    void    stopSampleStream();

    const Registry& registry() const { return registry_; }

signals:
    void interfaceConnected(QString source);
    void interfaceDisconnected(QString source);
    void interfaceError(QString source, QString message);
    void interfaceRate(QString source, double rateHz);
    void sentenceDetected(QString source, QString talker, QString formatter,
                          QString category, QStringList fieldNames,
                          QStringList fieldValues, double rateHz);
    void conversionAdded(QString talker, QString formatter,
                         QString deviceId, QString topic, QString qos);
    void conversionRemoved(QString talker, QString formatter);
    void conversionQoSChanged(QString talker, QString formatter, QString qos);
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
    std::map<QString, std::unique_ptr<nmea::Pipeline>> pipelines_;
    QTimer* poll_timer_;
    std::unique_ptr<nmea::ros::RosPublisher> ros_publisher_;

    struct RateTracker { quint64 last_count{0}; quint64 prev_count{0}; double rate_hz{0.0}; };
    std::map<std::string, RateTracker> rate_trackers_;   // por talker|formatter
    std::map<QString, RateTracker>     iface_rates_;     // por interfaz (source)

    // Monitor DDS: participante de solo-descubrimiento por dominio.
    eprosima::fastdds::dds::DomainParticipant* monitor_participant_{nullptr};
    std::unique_ptr<MonitorListener> monitor_listener_;
    struct SampleStream;                              // definido en el .cpp
    std::unique_ptr<SampleStream> sample_stream_;     // lector en vivo activo
    struct TopicAgg { std::string typeName; int pub{0}; int sub{0}; };
    std::map<std::string, TopicAgg> topic_agg_;
    int monitor_domain_{0};
};

}  // namespace nmea::ui
