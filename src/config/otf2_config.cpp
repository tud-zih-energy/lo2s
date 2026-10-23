// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/config/otf2_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <string>

#include <cstddef>

namespace lo2s
{
Otf2Config::Otf2Config(nitro::options::arguments& arguments)
: trace_path(arguments.get("output-trace")),
  definition_chunk_size(arguments.as<std::size_t>("definition-chunk-size")),
  event_chunk_size(arguments.as<std::size_t>("event-chunk-size"))
{
}

void Otf2Config::add_parser(nitro::options::parser& parser)
{
    auto& trace_options = parser.group("OTF-2 trace options");
    trace_options.option("output-trace", "Output trace directory.")
        .default_value("lo2s_trace_{DATE}")
        .env("LO2S_OUTPUT_TRACE")
        .short_name("o");

    trace_options
        .option("definition-chunk-size",
                "Size of the chunks in which OTF-2 definition are written. Increases this if OTF-2 "
                "trace writing fails due to chunk size.")
        .default_value(std::to_string(4 * 1024 * 1024))
        .metavar("CHUNKSIZE");

    trace_options
        .option("event-chunk-size",
                "Size of the chunks in which OTF-2 events are written. Increases this if OTF-2 "
                "trace writing fails due to chunk size..")
        .default_value(std::to_string(1024 * 1024))
        .metavar("CHUNKSIZE");
}

void to_json(nlohmann::json& j, const Otf2Config& config)
{
    j = nlohmann::json({
        { "trace_path", config.trace_path },
        { "definition_chunk_size", config.definition_chunk_size },
        { "event_chunk_size", config.event_chunk_size },
    });
}
} // namespace lo2s
