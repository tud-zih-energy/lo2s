#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/fwd.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <vector>

#include <cstdint>

namespace lo2s::perf
{

struct SyscallConfig
{
    static void add_parser(nitro::options::parser& parser);
    SyscallConfig(nitro::options::arguments& arguments);

    std::chrono::nanoseconds read_interval = std::chrono::nanoseconds(0);
    bool enabled = false;
    std::vector<int64_t> syscalls;
};

void to_json(nlohmann::json& j, const SyscallConfig& config);
} // namespace lo2s::perf
