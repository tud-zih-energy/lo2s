// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

#include <cstdint>

namespace lo2s::perf
{
struct SamplingConfig
{
    SamplingConfig(nitro::options::arguments& arguments);

    static void add_parser(nitro::options::parser& parser);

    void check();

    bool enabled = false;
    bool process_recording = false;

    std::uint64_t period = 0;
    std::string event;

    bool exclude_kernel = false;
    bool enable_callgraph = false;
    bool disassemble = false;
    bool use_pebs = false;
};

void to_json(nlohmann::json& j, const SamplingConfig& config);
} // namespace lo2s::perf
