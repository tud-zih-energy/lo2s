// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace lo2s
{
struct X86AdaptConfig
{

    X86AdaptConfig(nitro::options::arguments& arguments);
    static void parse_print_options(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);
    void check() const;

    bool enabled = false;
    std::vector<std::string> knobs;
};

void to_json(nlohmann::json& j, const X86AdaptConfig& config);
} // namespace lo2s
