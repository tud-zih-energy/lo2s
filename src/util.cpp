#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/types.hpp>
#include <lo2s/util.hpp>

#include <filesystem>

#include <fmt/core.h>

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>

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

std::string get_process_exe(Process process)
{
    auto proc_exe_filename = fmt::format("/proc/{}/exe", process.as_pid_t());
    char exe_cstr[PATH_MAX + 1];
    auto ret = readlink(proc_exe_filename.c_str(), exe_cstr, PATH_MAX);

    if (ret == -1)
    {
        Log::error() << "Failed to retrieve exe name for " << process << "!";
        throw std::runtime_error{ "Failed to retrive process exe from procfs" };
    }

    exe_cstr[ret] = '\0';
    return exe_cstr;
}

static std::string read_file(const std::filesystem::path& path)
{
    std::ifstream s;
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

std::string get_process_comm(Process process)
{
    auto proc_comm = std::filesystem::path{ "/proc" } / std::to_string(process.as_pid_t()) / "comm";
    try
    {
        return read_file(proc_comm);
    }
    catch (const std::ios::failure&)
    {
        Log::warn() << "Failed to get name for " << process;
        return fmt::format("[process {}]", process.as_pid_t());
    }
}

std::string get_task_comm(Process process, Thread thread)
{
    auto task_comm = std::filesystem::path{ "/proc" } / std::to_string(process.as_pid_t()) /
                     "task" / std::to_string(thread.as_pid_t()) / "comm";
    try
    {
        return read_file(task_comm);
    }
    catch (const std::ios::failure&)
    {
        Log::warn() << "Failed to get name for " << thread << " in " << process;
        return fmt::format("[thread {}]", thread.as_pid_t());
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

std::unordered_map<Thread, std::string> get_comms_for_running_threads()
{
    ExecutionScopeGroup& scope_group = ExecutionScopeGroup::instance();
    std::unordered_map<Thread, std::string> ret;
    std::filesystem::path proc("/proc");
    for (auto& entry : std::filesystem::directory_iterator(proc))
    {
        Process process;
        try
        {
            process = Process(std::stoi(entry.path().filename().string()));
        }
        catch (const std::logic_error&)
        {
            continue;
        }
        std::string name = get_process_comm(process);

        scope_group.add_process(process);

        Log::trace() << "mapping from /proc/" << process.as_pid_t() << ": " << name;
        ret.emplace(process.as_thread(), name);
        try
        {
            std::filesystem::path task(fmt::format("/proc/{}/task", process.as_pid_t()));
            for (auto& entry_task : std::filesystem::directory_iterator(task))
            {
                Thread thread;
                try
                {
                    thread = Thread(std::stoi(entry_task.path().filename().string()));
                }
                catch (const std::logic_error&)
                {
                    continue;
                }
                if (thread == process.as_thread())
                {
                    continue;
                }

                scope_group.add_thread(thread, process);

                name = get_task_comm(process, thread);
                Log::trace() << "mapping from /proc/" << process.as_pid_t() << "/"
                             << thread.as_pid_t() << ": " << name;
                ret.emplace(thread, name);
            }
        }
        catch (...)
        {
        }
    }
    return ret;
}

void try_pin_to_scope(ExecutionScope scope)
{
    cpu_set_t cpumask;
    CPU_ZERO(&cpumask);
    if (scope.is_thread())
    {
        // Copy affinity from mentioned thread
        sched_getaffinity(scope.as_thread().as_pid_t(), sizeof(cpumask), &cpumask);
    }
    else
    {
        CPU_SET(scope.as_cpu().as_int(), &cpumask);
    }
    auto ret = sched_setaffinity(0, sizeof(cpumask), &cpumask);
    if (ret != 0)
    {
        Log::error() << "sched_setaffinity failed with: " << make_system_error().what();
    }
}

Thread gettid()
{
    return Thread(syscall(SYS_gettid));
}
} // namespace lo2s
