#include <lo2s/config.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/util.hpp>

#include <filesystem>

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <ratio>

extern "C"
{
#include <sys/time.h>
}

namespace lo2s
{

Summary& summary()
{
    static Summary s;
    return s;
}

Summary::Summary()
: start_wall_time_(std::chrono::steady_clock::now()), num_wakeups_(0), thread_count_(0),
  exit_code_(0)
{
}

void Summary::register_process(pid_t pid)
{
    std::lock_guard<std::mutex> lock(pids_mutex_);
    pids_.emplace(pid);
}

void Summary::record_perf_wakeups(std::size_t num_wakeups)
{
    num_wakeups_ += num_wakeups;
}

void Summary::set_exit_code(int exit_code)
{
    exit_code_ = exit_code;
}

void Summary::add_thread()
{
    thread_count_++;
}

std::string pretty_print_bytes(std::size_t trace_size)
{
    double result_size = trace_size;
    std::size_t unit = 0;

    std::array<std::string, 5> units = { { "B", "KiB", "MiB", "GiB", "TiB" } };

    while (result_size > 1024)
    {
        result_size /= 1024;
        unit++;

        // We can not get higher than TiB so break here
        if (unit == units.size() - 1)
        {
            break;
        }
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << result_size << " " << units[unit];
    return out.str();
}

void Summary::set_trace_dir(const std::string& trace_dir)
{
    trace_dir_ = trace_dir;
}

void Summary::show()
{

    std::size_t trace_size;

    if (config().quiet)
    {
        return;
    }

    std::chrono::duration<double> wall_time = std::chrono::steady_clock::now() - start_wall_time_;

    std::chrono::duration<double> cpu_time = get_cpu_time();

    std::filesystem::recursive_directory_iterator it(trace_dir_), end;

    trace_size =
        std::accumulate(it, end, static_cast<size_t>(0),
                        [](std::size_t sum, const std::filesystem::directory_entry& entry) {
                            if (!std::filesystem::is_directory(entry))
                            {
                                return sum + std::filesystem::file_size(entry);
                            }
                            return sum;
                        });

    if (config().monitor_type == lo2s::MonitorType::PROCESS)
    {
        std::cout << "[ lo2s: ";
    }
    else
    {
        std::cout << "[ lo2s (system mode): ";
    }
    if (!config().command.empty())
    {
        for (const auto command : config().command)
        {
            std::cout << command << ' ';
        }

        std::cout << " (" << exit_code_ << "), ";
        std::cout << thread_count_ << " threads, ";
    }
    if (config().monitor_type == lo2s::MonitorType::CPU_SET)
    {
        std::cout << "monitored processes: " << pids_.size() << ", ";
    }
    std::cout << cpu_time.count() << "s CPU, ";
    std::cout << wall_time.count() << "s total ]\n";

    if (config().monitor_type == lo2s::MonitorType::PROCESS)
    {
        std::cout << "[ lo2s: ";
    }
    else
    {
        std::cout << "[ lo2s (system mode): ";
    }
    std::cout << num_wakeups_ << " wakeups, ";

    if (trace_dir_ != "")
    {
        std::cout << "wrote " << pretty_print_bytes(trace_size) << " " << trace_dir_;
    }

    std::cout << " ]\n";
}
} // namespace lo2s
