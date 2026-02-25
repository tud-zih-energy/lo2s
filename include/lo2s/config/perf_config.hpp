#pragma once

#include <lo2s/config/perf/block_io_config.hpp>
#include <lo2s/config/perf/group_config.hpp>
#include <lo2s/config/perf/posix_io_config.hpp>
#include <lo2s/config/perf/sampling_config.hpp>
#include <lo2s/config/perf/syscall_config.hpp>
#include <lo2s/config/perf/tracepoint_config.hpp>
#include <lo2s/config/perf/userspace_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>

#include <cstddef>

namespace lo2s::perf
{

struct Config
{
    static void add_parser(nitro::options::parser& parser);
    static void parse_print_options(nitro::options::arguments& arguments);
    Config(nitro::options::arguments& arguments);

    bool any_tracepoints() const
    {
        return !tracepoints.events.empty() || block_io.enabled || syscall.enabled;
    }

    bool any_perf() const
    {
        return block_io.enabled || group.enabled || userspace.enabled || sampling.enabled ||
               sampling.process_recording;
    }

    void check();

    SamplingConfig sampling;
    TracepointConfig tracepoints;
    BlockIOConfig block_io;
    PosixIOConfig posix_io;
    SyscallConfig syscall;

    counter::UserspaceConfig userspace;
    counter::GroupConfig group;

    int cgroup_fd = -1;
    std::size_t mmap_pages = 16;
    std::optional<clockid_t> clockid = std::nullopt;
};

void to_json(nlohmann::json& j, const perf::Config& config);
} // namespace lo2s::perf
