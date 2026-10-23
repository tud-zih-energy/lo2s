// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace lo2s
{
struct Otf2Config
{
    explicit Otf2Config(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    std::string trace_path;
    std::size_t definition_chunk_size;
    std::size_t event_chunk_size;
};

void to_json(nlohmann::json& j, const Otf2Config& config);
} // namespace lo2s
