#include <lo2s/config/rb_config.hpp>

#include <lo2s/time/time.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <chrono>

#include <cstdint>

namespace lo2s
{
RingbufConfig::RingbufConfig(nitro::options::arguments& arguments)
{
    socket_path = arguments.get("socket");
    injectionlib_path = arguments.get("ld-library-path");
    size = arguments.as<uint64_t>("ringbuf-size");

    read_interval = std::chrono::milliseconds(arguments.as<std::uint64_t>("ringbuf-read-interval"));
}

void RingbufConfig::add_parser(nitro::options::parser& parser)
{
    auto& rb_options = parser.group("ringbuffer event submit options");
    rb_options.option("socket", "Path to socket used for ringbuffer communication.")
        .default_value("/tmp/lo2s.socket")
        .metavar("PATH");
    rb_options.option("ringbuf-read-interval", "interval for readings of the ring buffer")
        .optional()
        .metavar("MSEC")
        .default_value("100");
    rb_options.option("ld-library-path", "path to the lo2s injection library")
        .optional()
        .metavar("PATH")
        .default_value(LO2S_INJECTIONLIB_PATH);

    rb_options.option("ringbuf-size", "Size of the injection library ring-buffer")
        .optional()
        .metavar("PAGES")
        .default_value("16");
}

void to_json(nlohmann::json& j, const RingbufConfig& config)
{
    j = nlohmann::json({ { "socket_path", config.socket_path },
                         { "injectionlib_path", config.injectionlib_path },
                         { "size", config.size },
                         { "read_interval", config.read_interval.count() } });
}
}; // namespace lo2s
