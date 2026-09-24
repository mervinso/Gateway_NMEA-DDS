// ¿Es el byte 3 un indicador de relleno? Se varía la longitud del cuerpo
// cambiando device_id y se mira si las opciones de encapsulación siguen el
// resto de división entre 4.
#include <cstdio>
#include <cstring>
#include <vector>

#include "NmeaDBTPubSubTypes.hpp"
#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>

using namespace nmea;
using namespace eprosima::fastdds::dds;

struct P {
    Parser p; ParseResult r{ParseResult::Incomplete};
    explicit P(std::string_view s) {
        for (char c : s) { r = p.consume(c); if (r != ParseResult::Incomplete) break; }
    }
    const SentenceView& v() const { return p.sentence(); }
};

template <typename PST, typename T>
static std::vector<unsigned char> ser(PST& pst, T& sample) {
    const uint32_t n = pst.calculate_serialized_size(&sample, XCDR2_DATA_REPRESENTATION);
    eprosima::fastdds::rtps::SerializedPayload_t pl(n + 16);
    pst.serialize(&sample, pl, XCDR2_DATA_REPRESENTATION);
    return std::vector<unsigned char>(pl.data, pl.data + pl.length);
}

int main() {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);
    const int64_t RECV = 1700000000000000000LL;

    P s("$SDDBT,036.5,f,011.1,M,,F*29");
    if (s.r != ParseResult::Complete) {
        P s2("$SDDBT,036.5,f,011.1,M,,F*29\r\n");
        if (s2.r != ParseResult::Complete) { std::printf("no parsea\n"); return 1; }
    }
    P sent("$SDDBT,036.5,f,011.1,M,,F*29\r\n");

    auto dyn_type = mapper.type_for("DBT");
    DynamicPubSubType dyn_pst(dyn_type);
    NmeaDBTPubSubType st_pst;

    std::printf("%-12s %5s  %-9s %-9s %-9s %s\n",
                "device_id", "bytes", "dyn_hdr", "gen_hdr", "cuerpo", "todo");
    for (const char* dev : {"s", "so", "sou", "soun", "sound", "sounde", "sounder-1"}) {
        auto dyn = mapper.map(sent.v(), dev, RECV);
        uint32_t mask = 0;
        dyn->get_uint32_value(mask, dyn->get_member_id_by_name("field_presence"));
        const auto a = ser(dyn_pst, dyn);

        NmeaDBT st;
        st.device_id(dev);
        st.talker("SD");
        st.recv_timestamp(RECV);
        st.field_presence(mask);
        st.depth_feet(36.5);
        st.feet_unit('f');
        st.depth_meters(11.1);
        st.meters_unit('M');
        st.depth_fathoms(0.0);
        st.fathoms_unit('F');
        const auto b = ser(st_pst, st);

        const bool body = a.size() == b.size() &&
                          std::memcmp(a.data() + 4, b.data() + 4, a.size() - 4) == 0;
        const bool all = a.size() == b.size() &&
                         std::memcmp(a.data(), b.data(), a.size()) == 0;
        std::printf("%-12s %5zu  %02X%02X%02X%02X   %02X%02X%02X%02X   %-9s %s\n",
                    dev, a.size(), a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3],
                    body ? "igual" : "DISTINTO", all ? "igual" : "distinto");
    }
    return 0;
}
