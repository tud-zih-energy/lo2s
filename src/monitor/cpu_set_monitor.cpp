#include <lo2s/monitor/cpu_set_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <csignal>

extern "C" {
#include <sys/wait.h>
}

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
        auto ret = monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                                     std::forward_as_tuple(cpu.id, trace_));
        assert(ret.second);
        (void)ret;
    }
}

void CpuSetMonitor::run()
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);

    auto ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
    if (ret)
    {
        Log::error() << "Failed to set pthread_sigmask: " << ret;
        throw std::runtime_error("Failed to set pthread_sigmask");
    }

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.start();
    }

    trace_.register_tids(read_all_tid_exe());

    if(config().command.size() == 0)
    {
        int sig;
        ret = sigwait(&ss, &sig);
    }
    else
    {
        auto pid = fork();

        if(pid == -1)
        {
            Log::error() << "Fork failed.";
            throw_errno();
        }
        else if(pid == 0)
        {
            std::vector<char*> tmp;

            std::transform(config().command.begin(), config().command.end(), std::back_inserter(tmp),
                   [](const std::string& s) {
                       char* pc = new char[s.size() + 1];
                       std::strcpy(pc, s.c_str());
                       return pc;
                   });
            tmp.push_back(nullptr);

            execvp(tmp[0], &tmp[0]);

            // should not be executed -> exec failed, let's clean up anyway.
            for (auto cp : tmp)
            {
                delete[] cp;
            }

            Log::error() << "Could not execute command: " << config().command.at(0);
            exit(errno);
        }
        else
        {
            //Wait for process termination
            if(waitpid(pid, NULL, 0) == -1)
            {
                ret = -1;
            }
        }
    }
    if (ret)
    {
        throw make_system_error();
    }

    trace_.register_tids(read_all_tid_exe());

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.stop();
    }

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.merge_trace();
    }
    throw std::system_error(0, std::system_category());
}
}
}
