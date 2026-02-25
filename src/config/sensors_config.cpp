#include <lo2s/config/sensors_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>

namespace lo2s
{
SensorsConfig::SensorsConfig(nitro::options::arguments& arguments)
{
    enabled = arguments.given("sensors");
    read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("tracepoint-read-interval"));
}

void SensorsConfig::add_parser(nitro::options::parser& parser)
{
    auto& sensors_options = parser.group("sensors options");
    sensors_options.toggle("sensors", "Record sensors using libsensors.").short_name("S");
    sensors_options
        .option("sensors-read-interval", "Time in milliseconds between readouts of sensors events")
        .default_value("100")
        .metavar("MSEC");
}

void SensorsConfig::check()
{
#ifndef HAVE_SENSORS
    if (enabled)
    {
        lo2s::Log::fatal() << "lo2s was built without support for libsensors; "
                              "cannot request sensor recording.\n";
        std::exit(EXIT_FAILURE);
    }
#endif
}

void to_json(nlohmann::json& j, const SensorsConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled } });
}
} // namespace lo2s
