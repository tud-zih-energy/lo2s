// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/rb/reader.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/resolvers/manual_function_resolver.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/process.hpp>

#include <otf2xx/chrono/time_point.hpp>

#include <map>
#include <memory>
#include <string>

#include <cstdint>

namespace lo2s::monitor
{

class GPUMonitor : public PollMonitor
{
public:
    GPUMonitor(trace::Trace& trace, int fd);

    void finalize_thread() override;
    void monitor(int fd) override;

    std::string group() const override
    {
        return "lo2s::GPUMonitor";
    }

    void emplace_resolvers(Resolvers& resolvers)
    {
        try
        {
            resolvers.gpu_function_resolvers[process_].emplace(
                Mapping(Address(0), Address(highest_func_ + 1), 0),
                std::make_shared<ManualFunctionResolver>("gpu kernels", functions_));
        }
        catch (lo2s::Range::Error& e)
        {
            Log::warn() << "Could not create GPU function resolver: " << e.what();
        }
    }

private:
    static constexpr int CCTX_LEVEL_PROCESS = 1;
    static constexpr int CCTX_LEVEL_KERNEL = 2;

    RingbufReader ringbuf_reader_;
    Process process_;
    perf::time::Converter& time_converter_;
    otf2::chrono::time_point last_tp_;

    LocalCctxTree& local_cctx_tree_;
    std::map<Address, std::string> functions_;
    uint64_t highest_func_ = 0;
};
} // namespace lo2s::monitor
