/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018, Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/rb/reader.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace monitor
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
        resolvers.gpu_function_resolvers[process_].emplace(
            Mapping(Address(0), Address(highest_func_ + 1), 0),
            std::make_shared<ManualFunctionResolver>("gpu kernels", functions_));
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
} // namespace monitor
} // namespace lo2s
