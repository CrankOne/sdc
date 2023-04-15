#include "sdc.hh"

struct ChannelCalibration {
    std::string label;
    int background;
    float scale;
    double covariance;
};

namespace sdc {

template<>
struct CalibDataTraits<ChannelCalibration> {
    static constexpr auto typeName = "channels-calib";
    template<typename T=ChannelCalibration> using Collection=std::vector<T>;

    template<typename T=ChannelCalibration>
    static inline void collect( Collection<T> & col
                              , const T & item
                              , const aux::MetaInfo & mi
                              ) { col.push_back(item); }

    static ChannelCalibration
            parse_line( const std::string & line
                      , const aux::MetaInfo & m
                      );
};

ChannelCalibration CalibDataTraits<ChannelCalibration>::parse_line(
        const std::string & line, const aux::MetaInfo & mi ) {
    // subject instance
    ChannelCalibration item;
    // tokenize line
    std::list<std::string> tokens = aux::tokenize(line);
    // use columns order provided in beforementioned `columns=' metadata
    // line to get the proxy object (a "CSV line") for easy by-column
    // retrieval
    aux::ColumnsOrder::CSVLine csv
            = mi.get<sdc::aux::ColumnsOrder>("columns").interpret(tokens);
    // now, once can set item's fields like
    item.label      = csv("label");
    item.background = csv("background", 0);
    item.scale      = csv("scale",      -1.0);
    item.covariance = csv("covariance", std::nan("0"));
    return item;
}

}  // namespace sdc

int main (int argc, char* argv[]) {
    if(3 != argc) return 1;
    int k = std::stoi(argv[2]);
    std::vector<ChannelCalibration> entries
        = sdc::load_from_fs<int, ChannelCalibration>(argv[1], k);
    std::cout << "Loaded " << entries.size() << " updates for key #"
        << k << " from dir \"" << argv[1] << "\":" << std::endl;
    for(ChannelCalibration & entry : entries) {
        std::cout << "  " << entry.label
                  << ": background="
                  << entry.background
                  << ", scale=" << entry.scale
                  << ", cov=" << entry.covariance
                  << std::endl;
    }
    return 0;
}

