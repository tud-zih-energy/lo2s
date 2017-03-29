#include <lo2s/monitor/global_monitor.hpp>

#include <lo2s/topology.hpp>

namespace lo2s
{
namespace monitor
{
GlobalMonitor::GlobalMonitor(const MonitorConfig& config) : MainMonitor(config)
{
    for (const auto& cpu : Topology::instance().cpus())
    {
        Log::debug() << "Create cstate recorder for cpu #" << cpu.id;
//        monitors_.emplace_back(cpu.id, config, trace_, mc, time_converter);
    }
}

GlobalMonitor::~GlobalMonitor()
{
}

void GlobalMonitor::run()
{
}
}
}