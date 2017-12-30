#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/iterator_range.hpp>

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <cstdint>
#include <ctime>
extern "C" {
#include <limits.h>
#include <sys/syscall.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{

std::size_t get_page_size()
{
    static std::size_t page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}
std::pair<double, double> get_process_times()
{
    int ticks_per_second = sysconf(_SC_CLK_TCK);
    struct tms buffer;
    double wall_time = times(&buffer);
    if (wall_time == -1)
    {
        return std::make_pair(0.0, 0.0);
    }
    double cpu_time = buffer.tms_utime + buffer.tms_stime + buffer.tms_cutime + buffer.tms_cstime;
    return std::make_pair(wall_time / ticks_per_second, cpu_time / ticks_per_second);
}
std::string get_process_exe(pid_t pid)
{
    auto proc_exe_filename = (boost::format("/proc/%d/exe") % pid).str();
    char exe_cstr[PATH_MAX + 1];
    auto ret = readlink(proc_exe_filename.c_str(), exe_cstr, PATH_MAX);
    if (ret != -1)
    {
        exe_cstr[ret] = '\0';
        return exe_cstr;
    }
    auto proc_comm_filename = (boost::format("/proc/%d/comm") % pid).str();
    std::ifstream proc_comm_stream;
    proc_comm_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        proc_comm_stream.open(proc_comm_filename);
        std::string comm;
        proc_comm_stream >> comm;
        return (boost::format("[%s]") % comm).str();
    }
    catch (const std::ios::failure&)
    {
    }
    return "<unknown>";
}

std::string get_datetime()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

int32_t get_task_last_cpu_id(std::istream& proc_stat)
{
    proc_stat.seekg(0);
    for (int i = 0; i < 38; i++)
    {
        std::string ignore;
        std::getline(proc_stat, ignore, ' ');
    }
    int32_t cpu_id = -1;
    proc_stat >> cpu_id;
    return cpu_id;
}
std::size_t get_perf_event_mlock()
{
    std::fstream mlock_kb_file;
    std::size_t mlock_kb;
    try
    {
        mlock_kb_file.exceptions(std::fstream::failbit | std::fstream::badbit);
        mlock_kb_file.open("/proc/sys/kernel/perf_event_mlock_kb", std::fstream::in);
        mlock_kb_file >> mlock_kb;
        mlock_kb_file.close();
        return mlock_kb * 1024;
    }
    catch (std::exception& e)
    {
        Log::trace() << "Reading /proc/sys/kernel/perf_event_mlock_kb failed: " << e.what();
        return 516 * 1024;
    }
}
std::unordered_map<pid_t, std::string> read_all_tid_exe()
{
    std::unordered_map<pid_t, std::string> ret;
    boost::filesystem::path proc("/proc");
    for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(proc), {}))
    {
        pid_t pid;
        try
        {
            pid = std::stoi(entry.path().filename().string());
        }
        catch (const std::logic_error&)
        {
            continue;
        }
        std::string name = get_process_exe(pid);
        Log::trace() << "mapping from /proc/" << pid << ": " << name;
        ret.emplace(pid, name);
        try
        {
            boost::filesystem::path task((boost::format("/proc/%d/task") % pid).str());
            for (auto& entry_task :
                 boost::make_iterator_range(boost::filesystem::directory_iterator(task), {}))
            {
                pid_t tid;
                try
                {
                    tid = std::stoi(entry_task.path().filename().string());
                }
                catch (const std::logic_error&)
                {
                    continue;
                }
                if (tid == pid)
                {
                    continue;
                }
                Log::trace() << "mapping from /proc/" << pid << "/" << tid << ": " << name;
                ret.emplace(tid, name);
            }
        }
        catch (...)
        {
        }
    }
    return ret;
}

void try_pin_to_cpu(int cpu, pid_t pid = 0)
{
    cpu_set_t cpumask;
    CPU_ZERO(&cpumask);
    CPU_SET(cpu, &cpumask);
    auto ret = sched_setaffinity(pid, sizeof(cpumask), &cpumask);
    if (ret != 0)
    {
        Log::error() << "sched_setaffinity failed with: " << make_system_error().what();
    }
}

pid_t gettid()
{
    return syscall(SYS_gettid);
}
}
