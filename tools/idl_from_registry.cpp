// tools/idl_from_registry.cpp — emite el IDL del brazo generado DESDE el registro.
//
// El brazo estático (obligación 4 del Cap. 8) tiene que declarar exactamente lo
// mismo que el brazo dinámico construye en tiempo de ejecución, o la compuerta
// de equivalencia CDR falla. Escribir ese IDL a mano es pedir que los dos
// diverjan: basta que alguien añada un campo a `Registry.cpp` y no toque el
// `.idl` para que la compuerta falle meses después, señalando al sitio
// equivocado.
//
// Así que no se escribe a mano. El IDL se emite desde `Registry::builtin()`, la
// misma estructura de la que el `Mapper` construye el `DynamicType`. Los dos
// brazos quedan derivados de una sola fuente, que es el mismo argumento por el
// que la partición warm/cold se calcula una vez sola del lado del análisis.
//
// Los cuatro miembros de cabecera van primero y en el mismo orden que
// `add_common_header()`: el orden de los miembros es parte del CDR.
//
// Committear el IDL emitido invita a que envejezca, asi que hay un modo
// `--check` que lo regenera en memoria y compara: falla si algun fichero falta
// o difiere. Es el mismo patron que `scripts/build_figures.sh --check` usa del
// lado de la tesis para los diagramas.
//
// Uso:
//   idl_from_registry <directorio-de-salida> [formatter ...]
//   idl_from_registry --check <directorio> [formatter ...]
//
// Sin formatters emite los cuatro de la escalera de aridad mas GGA, que es lo
// que la campaña necesita.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "registry/Registry.hpp"

using namespace nmea;

namespace {

// Tipos IDL correspondientes a los del registro. Si se añade un FieldType y no
// se añade aquí, el switch sin default hace que el compilador lo diga.
const char* idl_type(FieldType t) {
    switch (t) {
        case FieldType::Float64: return "double";
        case FieldType::Int32:   return "long";
        case FieldType::UInt32:  return "unsigned long";
        case FieldType::Char:    return "char";
        case FieldType::String:  return "string";
    }
    return "double";
}

constexpr const char* HEADER = R"(// GENERADO POR tools/idl_from_registry -- NO EDITAR A MANO.
//
// Emitido desde Registry::builtin(), la misma estructura de la que el Mapper
// construye el DynamicType del brazo dinámico. Los dos brazos derivan de una
// fuente única para que no puedan divergir: un campo añadido al registro y no
// al IDL haría fallar la compuerta de equivalencia CDR meses después,
// señalando al sitio equivocado.
//
// Sin @optional deliberadamente. La presencia va en field_presence, porque Fast
// DDS 3.6.2 no honra is_optional en DynamicData y el brazo dinámico no podría
// igualar a uno generado que sí codificara la ausencia (tesis §8.5.3).
)";

// El texto IDL de un formatter. Cadena vacía si el formatter no está en el
// registro o si declara más campos de los que la máscara puede llevar.
std::string emit(const Registry& reg, const std::string& f, std::string& error) {
    const SentenceDef* def = reg.lookup(f);
    if (!def) { error = "formatter desconocido en el registro: " + f; return {}; }
    if (def->fields.size() > 32) {
        error = f + " declara " + std::to_string(def->fields.size()) +
                " campos y field_presence solo tiene 32 bits";
        return {};
    }
    std::string o = HEADER;
    o += "\nstruct Nmea" + f + " {\n";
    o += "    // Cabecera. El orden importa: es parte del CDR, y debe coincidir\n";
    o += "    // con add_common_header() del Mapper.\n";
    o += "    @key string device_id;\n";
    o += "    string talker;\n";
    o += "    long long recv_timestamp;\n";
    o += "    unsigned long field_presence;\n\n";
    o += "    // Campos de sensor, en el orden del registro.\n";
    for (const auto& fd : def->fields)
        o += std::string("    ") + idl_type(fd.type) + " " + fd.name + ";\n";
    o += "};\n";
    return o;
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    bool check = false;
    if (!args.empty() && args.front() == "--check") { check = true; args.erase(args.begin()); }

    if (args.empty()) {
        std::fprintf(stderr,
                     "uso: %s [--check] <directorio> [formatter ...]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path dir = args.front();
    std::vector<std::string> wanted(args.begin() + 1, args.end());
    if (wanted.empty()) wanted = {"ROT", "RSA", "DBT", "VHW", "GGA"};

    if (!check) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    const Registry reg = Registry::builtin();
    int stale = 0;

    for (const std::string& f : wanted) {
        std::string error;
        const std::string text = emit(reg, f, error);
        if (text.empty()) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }

        const std::filesystem::path path = dir / ("Nmea" + f + ".idl");
        if (check) {
            const std::string on_disk = read_file(path);
            if (on_disk.empty()) {
                std::printf("  FALTA      %s\n", path.c_str());
                ++stale;
            } else if (on_disk != text) {
                std::printf("  DESFASADO  %s -- el registro cambio y el IDL no\n",
                            path.c_str());
                ++stale;
            } else {
                std::printf("  ok         %s\n", path.c_str());
            }
            continue;
        }

        std::ofstream o(path, std::ios::binary);
        if (!o) { std::fprintf(stderr, "no se pudo escribir %s\n", path.c_str()); return 1; }
        o << text;
        if (!o) { std::fprintf(stderr, "fallo al escribir %s\n", path.c_str()); return 1; }
        std::printf("  %-14s %2zu campos -> %s\n", f.c_str(),
                    reg.lookup(f)->fields.size(), path.c_str());
    }

    if (check) {
        if (stale) {
            std::fprintf(stderr,
                         "\n%d fichero(s) IDL desfasados respecto al registro. Regenerar con:\n"
                         "    ./build/idl_from_registry %s\n", stale, dir.c_str());
            return 1;
        }
        std::printf("\nel IDL coincide con el registro\n");
        return 0;
    }
    std::printf("%zu fichero(s) IDL emitido(s) desde el registro\n", wanted.size());
    return 0;
}
