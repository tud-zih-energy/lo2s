#pragma once

#include <lo2s/monitor/fd_monitor.hpp>

#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/tracepoint/switch_writer.hpp>
#include <lo2s/trace/fwd.hpp>

extern "C" {
#include <sched.h>
}

namespace lo2s
{
namespace monitor
{
class CpuSwitchMonitor : public FdMonitor
{
public:
    CpuSwitchMonitor(int cpu, const MonitorConfig& config, trace::Trace& trace)
    : switch_writer_(cpu, config, trace)
    {
        add_fd(switch_writer_.fd());
    }

    void initialize_thread() override
    {
        cpu_set_t cpumask;
        CPU_ZERO(&cpumask);
        CPU_SET(cpu_, &cpumask);
        sched_setaffinity(0, sizeof(cpumask), &cpumask);
    }

    void monitor() override
    {
        switch_writer_.read();
    }

private:
    int cpu_;

    perf::tracepoint::SwitchWriter switch_writer_;
};
}
}
