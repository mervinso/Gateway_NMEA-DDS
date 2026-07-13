#include "pipeline/Pipeline.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>

#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"
#include "pipeline/PublishPlan.hpp"

namespace nmea {

using namespace eprosima::fastdds::dds;

// ---------------------------------------------------------------------------
// DDS context — todo lo que crea un Pipeline, con cleanup en orden correcto.
// ---------------------------------------------------------------------------
struct DdsCtx {
    struct WriterEntry {
        Topic*                    topic{nullptr};
        DataWriter*               writer{nullptr};
        TypeSupport               ts;
        DynamicType::_ref_type    dyn_type;  // retiene el tipo vivo (D6)
        Pipeline::QosSettings     qos{};     // QoS con la que se creó el writer
    };

    DomainParticipant* participant{nullptr};
    Publisher*         publisher{nullptr};
    std::unordered_map<std::string, WriterEntry> writers;  // key = formatter o "raw"

    // Traduce QosSettings neutrales a un DataWriterQos de Fast DDS.
    DataWriterQos build_writer_qos(const Pipeline::QosSettings& q) const {
        DataWriterQos wq = DATAWRITER_QOS_DEFAULT;
        wq.reliability().kind = q.reliable ? RELIABLE_RELIABILITY_QOS
                                           : BEST_EFFORT_RELIABILITY_QOS;
        if (q.transient_local) {
            wq.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
            wq.history().kind    = KEEP_LAST_HISTORY_QOS;
            wq.history().depth   = 1;
        } else {
            wq.durability().kind = VOLATILE_DURABILITY_QOS;
        }
        if (q.deadline_ms > 0) {
            wq.deadline().period = Duration_t(q.deadline_ms / 1000,
                    static_cast<uint32_t>((q.deadline_ms % 1000) * 1000000));
        }
        if (q.lifespan_ms > 0) {
            wq.lifespan().duration = Duration_t(q.lifespan_ms / 1000,
                    static_cast<uint32_t>((q.lifespan_ms % 1000) * 1000000));
        }
        return wq;
    }

    // Devuelve la entrada completa (incluye dyn_type para crear DynamicData).
    WriterEntry* get_entry(const std::string& key) {
        auto it = writers.find(key);
        return (it != writers.end()) ? &it->second : nullptr;
    }

    // Obtiene o crea el DataWriter para el formatter dado.
    DataWriter* get_or_create(const Mapper& mapper,
                              const Mapper::SentenceInfo& info,
                              const Pipeline::QosSettings& wqos) {
        const std::string key = info.formatter.empty() ? "raw" : info.formatter;
        auto it = writers.find(key);
        if (it != writers.end()) {
            const auto& c = it->second.qos;
            const bool same = c.reliable == wqos.reliable
                           && c.transient_local == wqos.transient_local
                           && c.deadline_ms == wqos.deadline_ms
                           && c.lifespan_ms == wqos.lifespan_ms;
            if (same) return it->second.writer;
            // La QoS cambió. Reliability/Durability son inmutables en DDS, así que
            // se destruye el writer y se recrea con la nueva QoS (lag de un ciclo).
            if (it->second.writer) publisher->delete_datawriter(it->second.writer);
            if (it->second.topic)  participant->delete_topic(it->second.topic);
            writers.erase(it);
        }

        DynamicType::_ref_type dyn_type = info.formatter.empty()
                ? mapper.raw_sentence_type()
                : mapper.type_for(info.formatter);

        // TypeSupport toma ownership compartido (es un shared_ptr).
        // device_id es @key (D3/D5): dejamos que Fast DDS compute la clave de
        // instancia. (El SEGV histórico era por pasar data.get() a write() en vez
        // de &data, no por el key — ya corregido.)
        TypeSupport ts(new DynamicPubSubType(dyn_type));
        participant->register_type(ts, info.type_name);

        WriterEntry entry;
        entry.ts       = ts;
        entry.dyn_type = dyn_type;  // mantiene el DynamicType vivo
        entry.qos      = wqos;      // recuerda la QoS para detectar cambios
        entry.topic  = participant->create_topic(
                info.topic_name, info.type_name, TOPIC_QOS_DEFAULT);
        if (!entry.topic) return nullptr;

        entry.writer = publisher->create_datawriter(entry.topic, build_writer_qos(wqos));
        if (!entry.writer) {
            participant->delete_topic(entry.topic);
            return nullptr;
        }

        DataWriter* w = entry.writer;
        writers.emplace(key, std::move(entry));
        return w;
    }

    // Elimina los writers cuyo formatter ya no está en la allowlist `active`.
    void reconcile(const std::vector<std::string>& active) {
        for (auto it = writers.begin(); it != writers.end(); ) {
            if (std::find(active.begin(), active.end(), it->first) == active.end()) {
                if (it->second.writer) publisher->delete_datawriter(it->second.writer);
                if (it->second.topic)  participant->delete_topic(it->second.topic);
                it = writers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void cleanup() {
        for (auto& [k, e] : writers) {
            if (e.writer) publisher->delete_datawriter(e.writer);
            if (e.topic)  participant->delete_topic(e.topic);
            // pub_type destroyed by unique_ptr
        }
        writers.clear();
        if (publisher)   participant->delete_publisher(publisher);
        if (participant) DomainParticipantFactory::get_instance()
                                 ->delete_participant(participant);
    }
};

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------
Pipeline::Pipeline(Config cfg) : cfg_(std::move(cfg)) {}

Pipeline::~Pipeline() { stop(); }

void Pipeline::start() {
    if (state_.load() == State::Running) return;
    stop_.store(false);
    ok_.store(0);
    err_.store(0);
    thread_ = std::thread(&Pipeline::worker_loop, this);
}

void Pipeline::stop() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

Pipeline::State Pipeline::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

std::string Pipeline::error_message() const { return error_msg_; }

void Pipeline::worker_loop() {
    // ── DDS setup ──────────────────────────────────────────────────────────
    DdsCtx ctx;
    if (cfg_.publish_to_dds) {
        ctx.participant = DomainParticipantFactory::get_instance()
                ->create_participant(cfg_.domain_id, PARTICIPANT_QOS_DEFAULT);
        if (!ctx.participant) {
            error_msg_ = "DDS: failed to create DomainParticipant";
            state_.store(State::Error, std::memory_order_release);
            return;
        }
        ctx.publisher = ctx.participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        if (!ctx.publisher) {
            ctx.cleanup();
            error_msg_ = "DDS: failed to create Publisher";
            state_.store(State::Error, std::memory_order_release);
            return;
        }
    }

    Mapper mapper(*cfg_.registry);
    Parser parser;
    char   buf[256];

    // Señalamos Running solo aquí: DDS ya está listo, loop a punto de comenzar.
    state_.store(State::Running, std::memory_order_release);

    // ── Loop principal ─────────────────────────────────────────────────────
    while (!stop_.load(std::memory_order_relaxed)) {
        const ssize_t n = cfg_.source->read(buf, sizeof(buf), 100);
        if (n < 0) {
            error_msg_ = "source disconnected";
            state_.store(State::Error, std::memory_order_release);
            goto cleanup;
        }
        for (ssize_t i = 0; i < n; ++i) {
            const ParseResult r = parser.consume(buf[i]);
            if (r == ParseResult::Complete) {
                ++ok_;
                const auto& sv   = parser.sentence();
                const auto  info = mapper.resolve(sv.address);

                // talker ya viene resuelto en SentenceInfo (mismo cálculo que el Mapper).
                const std::string talker = info.talker;

                // Callback de preview (hilo worker → main thread via invokeMethod).
                if (cfg_.on_sentence) {
                    const SentenceDef* def = info.formatter.empty()
                            ? nullptr : cfg_.registry->lookup(info.formatter);
                    std::vector<std::string> names, values;
                    if (def) {
                        const std::size_t n = std::min(sv.fields.size(), def->fields.size());
                        for (std::size_t i = 0; i < n; ++i) {
                            names.push_back(def->fields[i].name);
                            values.push_back(std::string(sv.fields[i]));
                        }
                    }
                    cfg_.on_sentence(talker, info.formatter,
                                     category_name(def ? def->category : Category::GPS),
                                     std::move(names), std::move(values));
                }

                // Publicar solo si DDS está activo.
                if (cfg_.publish_to_dds) {
                    if (cfg_.plan) {
                        // Modo selectivo: solo tramas habilitadas, clave por sensor.
                        // Las sentencias raw (sin formatter) no son convertibles en modo selectivo.
                        if (!info.formatter.empty()) {
                            if (auto tgt = cfg_.plan->resolve(talker, info.formatter)) {
                                DataWriter* w = ctx.get_or_create(mapper, info, tgt->qos);
                                if (w) {
                                    auto* entry = ctx.get_entry(info.formatter);
                                    if (entry) {
                                        auto data = DynamicDataFactory::get_instance()
                                                ->create_data(entry->dyn_type);
                                        if (data) {
                                            mapper.populate(data, sv, tgt->device_id);
                                            w->write(&data);
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        // Modo legacy: publica todo con device_id/qos globales.
                        DataWriter* w = ctx.get_or_create(mapper, info, cfg_.qos);
                        if (w) {
                            const std::string ekey =
                                    info.formatter.empty() ? "raw" : info.formatter;
                            auto* entry = ctx.get_entry(ekey);
                            if (entry) {
                                auto data = DynamicDataFactory::get_instance()
                                        ->create_data(entry->dyn_type);
                                if (data) {
                                    mapper.populate(data, sv, cfg_.device_id);
                                    w->write(&data);
                                }
                            }
                        }
                    }
                }
            } else if (r == ParseResult::ChecksumError) {
                ++err_;
            }
        }
        if (cfg_.publish_to_dds && cfg_.plan)
            ctx.reconcile(cfg_.plan->active_formatters());
    }
    state_.store(State::Stopped, std::memory_order_release);

cleanup:
    ctx.cleanup();
}

}  // namespace nmea
