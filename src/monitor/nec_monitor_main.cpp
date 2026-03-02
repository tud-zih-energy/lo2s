// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/nec_monitor_main.hpp>

#include <lo2s/log.hpp>

#include <filesystem>

extern "C"
{
    extern int ve_sysfs_path_info(int nodeid, char* ve_sysfs_path);
}

namespace lo2s
{
namespace nec
{

std::vector<Thread> NecMonitorMain::get_tasks_of(NecDevice device)
{
    char sysfs_path_str[PATH_MAX];
    memset(sysfs_path_str, 0, PATH_MAX);

    auto ret = ve_sysfs_path_info(device.as_int(), sysfs_path_str);
    if (ret == -1)
    {
        throw_errno();
    }

    std::filesystem::path sysfs_path(sysfs_path_str);
    sysfs_path /= "task_id_all";
    std::ifstream task_stream(sysfs_path);

    std::vector<Thread> threads;

    while (true)
    {
        pid_t pid;
        task_stream >> pid;
        if (!task_stream)
        {
            break;
        }
        threads.emplace_back(Thread(pid));
    }

    return threads;
}

NecMonitorMain::NecMonitorMain(trace::Trace& trace, NecDevice device)
: ThreadedMonitor(trace, fmt::format("{}", device)), trace_(trace), device_(device), stopped_(false)
{
    auto ret = ve_node_info(&nodeinfo_);
    if (ret == -1)
    {
        Log::error() << "Failed to get Vector Engine node information!";
        throw_errno();
    }
}

void NecMonitorMain::run()
{
    while (!stopped_)
    {
        auto threads = get_tasks_of(device_);
        for (auto monitor = monitors_.begin(); monitor != monitors_.end();)
        {
            if (std::find(threads.begin(), threads.end(), monitor->first) == threads.end())
            {
                monitor->second.stop();
                monitor = monitors_.erase(monitor);
            }
            else
            {
                monitor++;
            }
        }

        for (auto& thread : threads)
        {
            if (monitors_.count(thread))
            {
                continue;
            }

            auto ret = monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                         std::forward_as_tuple(thread, trace_, device_));
            if (ret.second)
            {
                ret.first->second.start();
            }
        }
        std::this_thread::sleep_for(config().nec_check_interval);
    }
}

void NecMonitorMain::stop()
{
    stopped_ = true;
    thread_.join();
}

void NecMonitorMain::finalize_thread()
{
    for (auto& monitor : monitors_)
    {
        monitor.second.stop();
    }
}
} // namespace nec
} // namespace lo2s
