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
#include <numeric>
#include <regex>
#include <set>

#include <cstdint>
#include <ctime>

extern "C"
{
#include <fcntl.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/timerfd.h>
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

int get_cgroup_mountpoint_fd(std::string cgroup)
{
    std::ifstream mtab("/proc/mounts");

    // This regex does not work when cgroupfs is mounted on a path containing whitespaces
    // But i think people that mount important Linux filesystems on paths containing
    // whitespaces should not be let anywhere near lo2s anyways
    // /proc/mounts format:
    // device mountpoint fs_type options freq passno
    std::regex cgroup_regex(R"((\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+))");
    std::smatch cgroup_match;
    std::string line;
    while (std::getline(mtab, line))
    {
        if (std::regex_match(line, cgroup_match, cgroup_regex))
        {
            // For the ancient cgroupfs mount point we have to use the one with perf_event
            // in the options
            if (cgroup_match[3].str() == "cgroup2" ||
                (cgroup_match[3].str() == "cgroup" && cgroup_match[4].str().find("perf_event")))
            {
                std::filesystem::path cgroup_path =
                    std::filesystem::path(cgroup_match[2].str()) / cgroup;
                int fd = open(cgroup_path.c_str(), O_RDONLY);

                if (fd != -1)
                {
                    return fd;
                }
            }
        }
    }
    return -1;
}

std::set<std::uint32_t> parse_list(std::string list)
{
    std::stringstream s;
    s << list;

    std::set<uint32_t> res;

    std::string part;
    while (std::getline(s, part, ','))
    {
        auto pos = part.find('-');
        if (pos != std::string::npos)
        {
            // is a range
            uint32_t from = std::stoi(part.substr(0, pos));
            uint32_t to = std::stoi(part.substr(pos + 1));

            for (auto i = from; i <= to; ++i)
                res.insert(i);
        }
        else
        {
            // single value
            res.insert(std::stoi(part));
        }
    }

    return res;
}

std::set<std::uint32_t> parse_list_from_file(std::filesystem::path file)
{
    std::ifstream list_stream(file);
    std::string list_string;
    list_stream >> list_string;

    if (list_stream)
    {
        return parse_list(list_string);
    }

    return std::set<std::uint32_t>();
}

std::vector<std::string> get_thread_cmdline(Thread thread)
{
    std::ifstream cmdline(fmt::format("/proc/{}/cmdline", thread.as_pid_t()));
    std::string cmdline_str;
    cmdline >> cmdline_str;

    const char* cmdline_c_str = cmdline_str.c_str();
    std::vector<std::string> args;
    while (cmdline_c_str < cmdline_str.c_str() + cmdline_str.length())
    {
        args.emplace_back(std::string(cmdline_c_str));
        cmdline_c_str += args.back().length() + 1;
    }
    return args;
}

std::string get_nec_thread_comm(Thread thread)
{
    // We can't use /task/{pid}/comm to get the name of a NEC process because that will contain the
    // name of the program offloader (ve_exec) instead of the program that is run. Instead, we have
    // to parse the command line of the offloader. Thankfully, the kernel always puts '--' before
    // the name of the program run.
    auto args = get_thread_cmdline(thread);
    for (std::size_t i = 0; i < args.size(); i++)
    {
        if (args[i] == "--")
        {
            if (i + 1 < args.size())
            {
                return fmt::format("{} ({})", args[i + 1], thread.as_pid_t());
            }
        }
    }

    // If no '--' is found, fall back to the complete commandline as a name
    return std::accumulate(args.begin(), args.end(), std::string(""));
}

struct rlimit initial_rlimit_fd()
{
    static struct rlimit current;

    if (current.rlim_cur == 0)
    {
        auto ret = getrlimit(RLIMIT_NOFILE, &current);

        if (ret == -1)
        {
            Log::warn() << "Could not read file descriptor resource limit!";
        }
    }

    return current;
}

void bump_rlimit_fd()
{
    struct rlimit highest;

    auto ret = getrlimit(RLIMIT_NOFILE, &highest);

    if (ret == -1)
    {
        Log::warn()
            << "Could not read file descriptor hard resource limit, lo2s might run out of file "
               "descriptors!";
        Log::warn()
            << "If you encounter issues, try to manually increase the file descriptor rlimit.";
        return;
    }

    highest.rlim_cur = highest.rlim_max;
    ret = setrlimit(RLIMIT_NOFILE, &highest);

    if (ret == -1)
    {
        Log::warn() << "Could not increase file descriptor resource limit, lo2s might run out of "
                       "file descriptors!";
        Log::warn() << "If you encounter issues, try to manually increase the file descriptor "
                       "resource limit.";
    }
}

int timerfd_from_ns(std::chrono::nanoseconds duration)
{
    int timerfd;
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));

    tspec.it_interval.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    tspec.it_interval.tv_nsec = (duration % std::chrono::seconds(1)).count();

    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (timerfd == -1)
    {
        throw_errno();
    }

    if (timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &tspec, NULL) == -1)
    {
        throw_errno();
    }
    return timerfd;
}
} // namespace lo2s
