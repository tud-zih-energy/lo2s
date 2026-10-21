// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

namespace lo2s
{

struct AcceleratorConfig
{
    AcceleratorConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    bool nvidia = false;
    bool openmp = false;
    bool hip = false;
};

void to_json(nlohmann::json& j, const AcceleratorConfig& config);
} // namespace lo2s
