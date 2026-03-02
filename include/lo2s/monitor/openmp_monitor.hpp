// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/rb/reader.hpp>
#include <lo2s/types/process.hpp>

#include <otf2xx/chrono/time_point.hpp>

#include <map>
#include <string>

#include <cstdint>

namespace lo2s::monitor
{

class OpenMPMonitor : public PollMonitor
{
public:
    OpenMPMonitor(trace::Trace& trace, int fd);

    void finalize_thread() override;

    void monitor(int fd) override;

    std::string group() const override
    {
        return "lo2s::OpenMPMonitor";
    }

private:
    void create_thread_writer(otf2::chrono::time_point tp, uint64_t thread);
    RingbufReader ringbuf_reader_;
    Process process_;
    trace::Trace& trace_;
    perf::time::Converter& time_converter_;

    std::map<uint64_t, otf2::chrono::time_point> last_tp_;

    std::map<uint64_t, LocalCctxTree*> local_cctx_trees_;

    static constexpr int CCTX_LEVEL_PROCESS = 1;
};
} // namespace lo2s::monitor
