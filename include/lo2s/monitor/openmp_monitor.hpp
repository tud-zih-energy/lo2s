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

#include "lo2s/helpers/fd.hpp"
#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/ringbuf.hpp>

namespace lo2s
{
namespace monitor
{

class OpenMPMonitor : public PollMonitor
{
public:
    OpenMPMonitor(trace::Trace& trace, Fd &&memory_fd);

    void finalize_thread() override;

    void on_stop() override
    {
        read_ringbuffer();
    }

    void on_readout_interval() override
    {
        read_ringbuffer();
    }

    void on_fd_ready([[maybe_unused]] WeakFd fd, [[maybe_unused]] int revents) override
    {
        throw std::runtime_error("OpenMPMonitor does not monitor any fd, but an fd has still become readable!");
    }

    std::string group() const override
    {
        return "lo2s::OpenMPMonitor";
    }

private:
    void read_ringbuffer();
    void create_thread_writer(otf2::chrono::time_point tp, uint64_t thread);

    Fd memory_fd_;
    RingbufReader ringbuf_reader_;
    Process process_;
    trace::Trace& trace_;
    perf::time::Converter& time_converter_;

    std::map<uint64_t, otf2::chrono::time_point> last_tp_;

    std::map<uint64_t, LocalCctxTree*> local_cctx_trees_;

    static constexpr int CCTX_LEVEL_PROCESS = 1;
};
} // namespace monitor
} // namespace lo2s
