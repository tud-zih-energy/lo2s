#include <lo2s/config/otf2_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

namespace lo2s
{
Otf2Config::Otf2Config(nitro::options::arguments& arguments)
: trace_path(arguments.get("output-trace"))
{
}

void Otf2Config::add_parser(nitro::options::parser& parser)
{
    auto& trace_options = parser.group("OTF-2 trace options");
    trace_options.option("output-trace", "Output trace directory.")
        .default_value("lo2s_trace_{DATE}")
        .env("LO2S_OUTPUT_TRACE")
        .short_name("o");
}

void to_json(nlohmann::json& j, const Otf2Config& config)
{
    j = nlohmann::json({ { "trace_path", config.trace_path } });
}
} // namespace lo2s
