/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
 *    Technische Universitaet Dresden, Germany
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

#include <chrono>

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/calling_context_manager.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace nec
{
class NecThreadMonitor : public PollMonitor
{
public:
    NecThreadMonitor(Thread thread, trace::Trace& trace, int device);

protected:
    std::string group() const override
    {
        return "nec::ThreadMonitor";
    }

    void finalize_thread() override;

    void monitor(int fd) override;

private:
  std::chrono::microseconds nec_read_interval_;
    otf2::writer::local& otf2_writer_;
    Thread nec_thread_;
    trace::Trace& trace_;
    int device_;
    perf::CallingContextManager cctx_manager_;
};
} // namespace monitor
} // namespace lo2s
