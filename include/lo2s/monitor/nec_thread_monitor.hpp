// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/nec_device.hpp>
#include <lo2s/types/thread.hpp>

#include <otf2xx/writer/local.hpp>

#include <chrono>
#include <string>

namespace lo2s::nec
{
class NecThreadMonitor : public monitor::PollMonitor, perf::counter::MetricWriter
{
public:
    NecThreadMonitor(Thread thread, trace::Trace& trace, NecDevice device);

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
    NecDevice device_;
    CallingContextType cctx_manager_;
};
} // namespace lo2s::nec
