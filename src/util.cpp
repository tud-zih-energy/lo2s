#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <fmt/core.h>

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <unordered_map>

#include <cstdint>
#include <ctime>

extern "C"
{
#include <limits.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
}

namespace lo2s
{

std::size_t get_page_size()
{
    static std::size_t page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}

std::chrono::duration<double> get_cpu_time()
{
    struct rusage usage, child_usage;
    timeval time;

    if (getrusage(RUSAGE_SELF, &usage) == -1)
    {
        return std::chrono::seconds(0);
    }

    if (getrusage(RUSAGE_CHILDREN, &child_usage) == -1)
    {
        return std::chrono::seconds(0);
    }

    // Add together the system and user CPU time of the process and all children
    timeradd(&usage.ru_utime, &usage.ru_stime, &time);

    timeradd(&time, &child_usage.ru_utime, &time);
    timeradd(&time, &child_usage.ru_stime, &time);

    return std::chrono::seconds(time.tv_sec) + std::chrono::microseconds(time.tv_usec);
}

std::string get_process_exe(pid_t pid)
{
    auto proc_exe_filename = fmt::format("/proc/{}/exe", pid);
    char exe_cstr[PATH_MAX + 1];
    auto ret = readlink(proc_exe_filename.c_str(), exe_cstr, PATH_MAX);

    if (ret == -1)
    {
        Log::error() << "Failed to retrieve exe name for pid=" << pid << "!";
        throw std::runtime_error{ "Failed to retrive process exe from procfs" };
    }

    exe_cstr[ret] = '\0';
    return exe_cstr;
}

static std::string read_file(const boost::filesystem::path& path)
{
    boost::filesystem::ifstream s;
    s.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    s.open(path);

    try
    {
        std::string result;
        s >> result;
        return result;
    }
    catch (const std::ios::failure&)
    {
        Log::error() << "Failed to read file: " << path;
        throw;
    }
}

std::string get_process_comm(pid_t pid)
{
    auto proc_comm = boost::filesystem::path{ "/proc" } / std::to_string(pid) / "comm";
    try
    {
        return read_file(proc_comm);
    }
    catch (const std::ios::failure&)
    {
        Log::warn() << "Failed to get name for process " << pid;
        return fmt::format("[process {}]", pid);
    }
}

std::string get_task_comm(pid_t pid, pid_t task)
{
    auto task_comm = boost::filesystem::path{ "/proc" } / std::to_string(pid) / "task" /
                     std::to_string(task) / "comm";
    try
    {
        return read_file(task_comm);
    }
    catch (const std::ios::failure&)
    {
        Log::warn() << "Failed to get name for task " << task << " in process " << pid;
        return fmt::format("[thread {}]", task);
    }
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

const struct ::utsname& get_uname()
{
    static struct instance_wrapper
    {
        instance_wrapper()
        {
            if (::uname(&uname) < 0)
            {
                throw_errno();
            }
        }
        struct ::utsname uname;
    } instance{};

    return instance.uname;
}

std::unordered_map<pid_t, std::string> get_comms_for_running_processes()
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
        std::string name = get_process_comm(pid);
        Log::trace() << "mapping from /proc/" << pid << ": " << name;
        ret.emplace(pid, name);
        try
        {
            boost::filesystem::path task(fmt::format("/proc/{}/task", pid));
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
                name = get_task_comm(pid, tid);
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

void try_pin_to_cpu(int cpu, pid_t pid)
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
} // namespace lo2s
