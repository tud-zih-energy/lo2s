// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/metric/plugin/metrics.hpp>
#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/metrics.hpp>
#endif
#ifdef HAVE_X86_ENERGY
#include <lo2s/metric/x86_energy/metrics.hpp>
#endif
#ifdef HAVE_SENSORS
#include <lo2s/metric/sensors/recorder.hpp>
#endif
#include <lo2s/monitor/io_monitor.hpp>
#include <lo2s/monitor/socket_monitor.hpp>
#include <lo2s/monitor/tracepoint_monitor.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/trace/trace.hpp>

#include <memory>
#include <vector>

namespace lo2s::monitor
{

class MainMonitor
{
public:
    MainMonitor();

    MainMonitor& operator=(MainMonitor&) = delete;
    MainMonitor& operator=(MainMonitor&&) = delete;
    MainMonitor(MainMonitor&) = delete;
    MainMonitor(MainMonitor&&) = delete;
    virtual ~MainMonitor();

protected:
    trace::Trace trace_;
    Resolvers resolvers_;
    metric::plugin::Metrics metrics_;
    std::vector<std::unique_ptr<TracepointMonitor>> tracepoint_monitors_;

    std::unique_ptr<SocketMonitor> socket_monitor_;
    std::unique_ptr<IoMonitor<perf::bio::Writer>> bio_monitor_;
#ifdef HAVE_X86_ADAPT
    std::unique_ptr<metric::x86_adapt::Metrics> x86_adapt_metrics_;
#endif
#ifdef HAVE_X86_ENERGY
    std::unique_ptr<metric::x86_energy::Metrics> x86_energy_metrics_;
#endif
#ifdef HAVE_SENSORS
    std::unique_ptr<metric::sensors::Recorder> sensors_recorder_;
#endif
};
} // namespace lo2s::monitor
