#pragma once

#include <lo2s/monitor/active_monitor.hpp>

#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/sample/writer.hpp>

namespace lo2s
{
namespace monitor
{
class CoreMonitor : public ActiveMonitor
{
public:
    CoreMonitor(int id, const MonitorConfig& config) : ActiveMonitor(config.read_interval)
    {
    }

    void initialize_thread() override
    {
    }
    void finalize_thread() override
    {
    }
    void monitor() override
    {
    }

private:
    int core_id_;

    //perf::sample::Writer sample_writer_;
};
}
}
