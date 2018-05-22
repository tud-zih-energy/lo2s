#include <lo2s/monitor/cpu_set_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <lo2s/monitor/dummy_monitor.hpp>
#include <lo2s/monitor/process_monitor_main.hpp>

#include <csignal>

namespace lo2s
{
namespace monitor
{
CpuSetMonitor::CpuSetMonitor() : MainMonitor()
{
    trace_.register_monitoring_tid(gettid(), "CpuSetMonitor", "CpuSetMonitor");

    for (const auto& cpu : Topology::instance().cpus())
    {
        Log::debug() << "Create cstate recorder for cpu #" << cpu.id;
        auto ret = switch_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                                     std::forward_as_tuple(cpu.id, trace_));
        assert(ret.second);

        counter_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                std::forward_as_tuple(cpu.id, *this, ret.first->second.location()));

        (void)ret;
    }
}

void CpuSetMonitor::run()
{
    sigset_t ss;
    if (config().command.empty() && config().pid == -1)
    {
        sigemptyset(&ss);
        sigaddset(&ss, SIGINT);

        auto ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
        if (ret)
        {
            Log::error() << "Failed to set pthread_sigmask: " << ret;
            throw std::runtime_error("Failed to set pthread_sigmask");
        }
    }
    for (auto& monitor_elem : switch_monitors_)
    {
        monitor_elem.second.start();
    }

    for (auto& monitor_elem : counter_monitors_)
    {
        monitor_elem.second.start();
    }
    trace_.register_tids(read_all_tid_exe());

    if (config().command.empty() && config().pid == -1)
    {
        int sig;
        auto ret = sigwait(&ss, &sig);
        if (ret)
        {
            throw make_system_error();
        }
    }
    else
    {
        try
        {
            DummyMonitor monitor;
            process_monitor_main(monitor);
        }
        catch (std::system_error& e)
        {
            if (e.code())
            {
                throw e;
            }
        }
    }

    trace_.register_tids(read_all_tid_exe());

    for (auto& monitor_elem : switch_monitors_)
    {
        monitor_elem.second.stop();
    }

    for (auto& monitor_elem : counter_monitors_)
    {
        monitor_elem.second.stop();
    }

    for (auto& monitor_elem : switch_monitors_)
    {
        monitor_elem.second.merge_trace();
    }
    throw std::system_error(0, std::system_category());
}
}
}
