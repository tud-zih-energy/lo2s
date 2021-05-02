#include <lo2s/monitor/cpu_set_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <lo2s/monitor/dummy_monitor.hpp>
#include <lo2s/monitor/process_monitor_main.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>

#include <filesystem>

#include <regex>

#include <csignal>

namespace lo2s
{
namespace monitor
{
CpuSetMonitor::CpuSetMonitor() : MainMonitor()
{
    trace_.add_monitoring_thread(gettid(), "CpuSetMonitor", "CpuSetMonitor");

    // Prefill Memory maps
    std::regex proc_regex("/proc/([0-9]+)");
    std::smatch pid_match;
    pid_t pid;

    const std::filesystem::path proc_path("/proc");
    if (config().sampling)
    {
        for (const auto& p : std::filesystem::directory_iterator(proc_path))
        {
            std::string path = p.path().string();
            if (std::regex_match(path, pid_match, proc_regex))
            {
                pid = std::stol(pid_match[1]);

                process_infos_.emplace(std::piecewise_construct, std::forward_as_tuple(pid),
                                       std::forward_as_tuple(Process(pid), false));
            }
        }
    }

    trace_.add_threads(get_comms_for_running_threads());

    for (const auto& cpu : Topology::instance().cpus())
    {
        Log::debug() << "Create cstate recorder for cpu #" << cpu.id;

        monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                          std::forward_as_tuple(ExecutionScope(Cpu(cpu.id)), *this, false));
    }
}

void CpuSetMonitor::run()
{
    sigset_t ss;
    if (config().command.empty() && config().process == Process::invalid())
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

    if (config().command.empty() && config().process == Process::invalid())
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

    trace_.add_threads(get_comms_for_running_threads());

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.stop();
    }

    throw std::system_error(0, std::system_category());
}
} // namespace monitor
} // namespace lo2s
