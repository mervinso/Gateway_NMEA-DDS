// src/instrument/PublishProbe.hpp — instrumentación de la ruta de publicación.
//
// Obligación 4 del Cap. 8 de la tesis: *"Publish-path instrumentation, behind a
// build option, default off. The measured binary must be the shipped binary,
// modulo one flag."*
//
// Mide el intervalo que RQ1 define como su resultado primario: desde que el
// parser devuelve una sentencia completa hasta que `DataWriter::write()`
// retorna, con `CLOCK_MONOTONIC_RAW`. Ese intervalo cubre los cinco pasos que
// §8.4 enumera —resolver, obtener writer, asignar muestra, poblar, escribir— y
// el paso 4 es el mecanismo que H4 pone a prueba.
//
// Tres propiedades que no son opcionales, y por qué:
//
//  1. **Con GATEWAY_INSTRUMENT apagado esto no existe.** Todos los métodos son
//     cuerpos vacíos `inline` y el compilador los elimina; no queda ni una
//     rama, ni un miembro, ni una llamada al reloj. Es lo que permite que el
//     binario medido sea el binario publicado salvo un flag.
//  2. **Nada de E/S dentro de la ventana.** Las muestras van a un vector
//     reservado por adelantado y se vuelcan al cerrar. Un instrumento que
//     escribe a disco por muestra mide el disco.
//  3. **Nada de asignaciones por muestra.** El formatter se copia a un arreglo
//     fijo, no a un `std::string`. Una asignación por muestra dentro de la
//     región cronometrada sería un costo del instrumento atribuido al
//     artefacto.
//
// El desbordamiento se cuenta, nunca se ignora: si la reserva se agota la
// muestra se descarta y `dropped()` lo dice. Una corrida que desbordó produce
// menos muestras de las que el manifiesto declara y queda anulada por
// `v4_incomplete`, que es exactamente lo que debe pasar.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#ifdef GATEWAY_INSTRUMENT
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#endif

namespace nmea {

#ifdef GATEWAY_INSTRUMENT

class PublishProbe {
public:
    struct Sample {
        std::uint64_t seq;              // posición de la sentencia dentro de la corrida
        std::int64_t  t0_ns;            // CLOCK_MONOTONIC_RAW al retornar el parser
        std::int64_t  publish_path_ns;  // t1 - t0, con t1 al retornar write()
        char          formatter[8];     // "GGA", "VNYMR", "" si desconocido
    };

    // `capacity` muestras reservadas de una vez. A 2 000 sentencias/s durante
    // 300 s son 600 000; el barrido de saturación puede pedir mucho más, y por
    // eso el desbordamiento se cuenta en vez de crecer el vector dentro de la
    // ventana.
    explicit PublishProbe(std::size_t capacity = 2'000'000) {
        samples_.reserve(capacity);
        capacity_ = capacity;
    }

    static std::int64_t now_ns() {
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
    }

    // Llamado justo tras el retorno del parser con Complete.
    std::int64_t start() const { return now_ns(); }

    // Llamado justo tras el retorno de write(). `t0` es lo que devolvió start().
    void record(std::int64_t t0, std::string_view formatter) {
        const std::int64_t t1 = now_ns();
        if (samples_.size() >= capacity_) { ++dropped_; return; }
        Sample s{};
        s.seq = seq_++;
        s.t0_ns = t0;
        s.publish_path_ns = t1 - t0;
        const std::size_t n = std::min(formatter.size(), sizeof(s.formatter) - 1);
        std::memcpy(s.formatter, formatter.data(), n);
        s.formatter[n] = '\0';
        samples_.push_back(s);
    }

    // Costo del par de relojes, medido en este host y en esta corrida. El
    // manifiesto lo pide como `timing.instrument_p99_ns` y la compuerta 2 exige
    // que quede por debajo de 500 ns en toda la campaña. Se mide con dos
    // lecturas consecutivas y nada en medio: es el suelo que el instrumento
    // añade a cada muestra.
    static double clock_pair_overhead_p99_ns(std::size_t reps = 10'000) {
        std::vector<std::int64_t> d;
        d.reserve(reps);
        for (std::size_t i = 0; i < reps; ++i) {
            const std::int64_t a = now_ns();
            const std::int64_t b = now_ns();
            d.push_back(b - a);
        }
        std::sort(d.begin(), d.end());
        const std::size_t idx = static_cast<std::size_t>(0.99 * (d.size() - 1));
        return static_cast<double>(d[idx]);
    }

    // Vuelca a CSV *después* de cerrar la ventana. CSV y no parquet a
    // propósito: escribir parquet exigiría Arrow dentro del binario medido, y
    // el binario medido debe diferir del publicado solo en un flag. El arnés
    // de corrida convierte a parquet antes de calcular MANIFEST.sha256.
    bool dump_csv(const std::string& path) const {
        std::FILE* f = std::fopen(path.c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "seq,formatter,ts,publish_path_ns\n");
        for (const Sample& s : samples_) {
            std::fprintf(f, "%llu,%s,%lld,%lld\n",
                         static_cast<unsigned long long>(s.seq), s.formatter,
                         static_cast<long long>(s.t0_ns),
                         static_cast<long long>(s.publish_path_ns));
        }
        return std::fclose(f) == 0;
    }

    std::size_t size() const { return samples_.size(); }
    std::uint64_t dropped() const { return dropped_; }
    const std::vector<Sample>& samples() const { return samples_; }
    void clear() { samples_.clear(); seq_ = 0; dropped_ = 0; }

    static constexpr bool enabled = true;

private:
    std::vector<Sample> samples_;
    std::size_t   capacity_{0};
    std::uint64_t seq_{0};
    std::uint64_t dropped_{0};
};

#else   // ── GATEWAY_INSTRUMENT apagado ────────────────────────────────────

// Misma interfaz, cuerpos vacíos. El compilador la elimina entera: sin
// miembros, sin ramas, sin llamadas al reloj en el camino caliente.
class PublishProbe {
public:
    explicit PublishProbe(std::size_t = 0) {}
    static std::int64_t now_ns() { return 0; }
    std::int64_t start() const { return 0; }
    void record(std::int64_t, std::string_view) const {}
    static double clock_pair_overhead_p99_ns(std::size_t = 0) { return 0.0; }
    bool dump_csv(const char*) const { return false; }
    std::size_t size() const { return 0; }
    std::uint64_t dropped() const { return 0; }
    void clear() {}

    static constexpr bool enabled = false;
};

#endif

}  // namespace nmea
