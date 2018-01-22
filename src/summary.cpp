#include <fstream>
#include <iostream>
#include <numeric>

#include <boost/filesystem.hpp>

#include <lo2s/config.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>
namespace lo2s
{
std::string Summary::trace_dir_ = "";
double Summary::start_cpu_time_ = 0;
double Summary::start_wall_time_ = 0;
long Summary::thread_count_ = 0;
long Summary::num_wakeups_ = 0;
boost::dynamic_bitset<> processes_ = boost::dynamic_bitset<>(get_max_pid() + 1);
int Summary::exit_code_ = 0;
Summary::Summary()
{
}
void Summary::set_pid(pid_t pid)
{
    processes_[pid] = 1;
}
void Summary::record_wakeups(int num_wakeups)
{
    static std::mutex mux;
    mux.lock();
    num_wakeups_ += num_wakeups;
    mux.unlock();
}
void Summary::start()
{
    std::pair<double, double> times = get_process_times();
    start_wall_time_ = times.first;
    start_cpu_time_ = times.second;
}
void Summary::set_trace_dir(std::string trace_dir)
{
    trace_dir_ = trace_dir;
}
void Summary::set_exit_code(int exit_code)
{
    exit_code_ = exit_code;
}
std::string pretty_print_bytes(int trace_size)
{
    double result_size = trace_size;
    int unit = 0;
    std::string units[5] = { "B", "KiB", "MiB", "GiB", "TiB" };

    for (; result_size > 1024; unit++, result_size /= 1024)
        ;

    if (unit > 4)
    {
        return std::to_string(trace_size) + "B";
    }

    // Rounding code;
    result_size = (int)(result_size / 0.01) * 0.01;
    std::ostringstream out;
    out << result_size;
    return out.str() + " " + units[unit];
}
void Summary::finalize_and_print()
{
    int trace_size;

    std::pair<double, double> times = get_process_times();
    double wall_time = times.first - start_wall_time_;
    double cpu_time = times.second - start_cpu_time_;

    boost::filesystem::recursive_directory_iterator it(trace_dir_), end;
    trace_size = std::accumulate(
        it, end, 0, [](long unsigned int sum, boost::filesystem::directory_entry& entry) {
            if (!boost::filesystem::is_directory(entry))
            {
                return sum + boost::filesystem::file_size(entry);
            }
            return sum;
        });
    std::cout << "[ lo2s: ";
    if (config().monitor_type == lo2s::MonitorType::PROCESS)
    {
        for (auto i = config().command.begin(); i != config().command.end(); ++i)
        {
            std::cout << *i << ' ';
        }
        std::cout << " (" << exit_code_ << "), ";
        std::cout << thread_count_ << " threads, ";
    }
    else
    {
        std::cout << "-a mode, ";
        std::cout << "monitored processes: " << processes_.count() << ", ";
    }
    std::cout << cpu_time << "s CPU, ";
    std::cout << wall_time << "s total ]\n";
    std::cout << "[ lo2s: ";
    std::cout << num_wakeups_ << " wakeups, ";
    if (trace_dir_ != "" && trace_size != -1)
    {
        std::cout << "wrote " << pretty_print_bytes(trace_size) << " " << trace_dir_;
    }
    std::cout << " ]\n";
}
void Summary::increase_thread_count()
{
    static std::mutex mux;
    mux.lock();
    thread_count_++;
    mux.unlock();
}
}
