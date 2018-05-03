#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/iterator_range.hpp>

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
#ifdef HAVE_DARWIN
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <sys/sysctl.h>
#endif
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

std::unordered_map<pid_t, std::string> read_all_tid_exe()
{
    std::unordered_map<pid_t, std::string> ret;

#ifdef HAVE_LINUX
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
#endif
#ifdef HAVE_DARWIN
    int maxArgumentSize = 0;
    size_t size = sizeof(maxArgumentSize);
    int sysctl_name[2] = { CTL_KERN, KERN_ARGMAX };

    if (sysctl(sysctl_name, 2, &maxArgumentSize, &size, NULL, 0) == -1)
    {
        Log::error() << "Cannot read max argument size, falling back to 4096.";
        maxArgumentSize = 4096; // Default
    }

    int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
    std::unique_ptr<struct kinfo_proc[]> info;
    size_t length;

    if (sysctl(mib, 3, NULL, &length, NULL, 0) < 0)
    {
        Log::error() << "Cannot read proc info size.";
        return ret;
    }

    info.reset(reinterpret_cast<struct kinfo_proc*>(new char[length]));

    if (sysctl(mib, 3, info.get(), &length, NULL, 0) < 0)
    {
        Log::error() << "Cannot read proc info.";
        return ret;
    }

    int count = length / sizeof(struct kinfo_proc);
    for (int i = 0; i < count; i++)
    {
        pid_t pid = info[i].kp_proc.p_pid;
        if (pid == 0)
        {
            continue;
        }
        size_t size = maxArgumentSize;
        std::vector<char> buffer(length);
        int sysctl_proc[3] = { CTL_KERN, KERN_PROCARGS2, pid };
        if (sysctl(sysctl_proc, 3, buffer.data(), &size, nullptr, 0) == 0)
        {
            std::string process(buffer.data() + sizeof(int));
            ret.emplace(pid, process);
        }
    }
#endif

    return ret;
}

void try_pin_to_cpu(int cpu, pid_t pid = 0)
{
#ifdef HAVE_LINUX
    cpu_set_t cpumask;
    CPU_ZERO(&cpumask);
    CPU_SET(cpu, &cpumask);
    auto ret = sched_setaffinity(pid, sizeof(cpumask), &cpumask);
    if (ret != 0)
    {
        Log::error() << "sched_setaffinity failed with: " << make_system_error().what();
    }
#else
    if (pid != 0)
    {
        Log::debug() << "Pinning other threads is not supported now.";
        return;
    }

    thread_affinity_policy_data_t policy_data = { cpu };
    thread_port_t threadport = pthread_mach_thread_np(pthread_self());
    thread_policy_set(threadport, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data,
                      THREAD_AFFINITY_POLICY_COUNT);

#endif
}

pid_t gettid()
{
#ifdef HAVE_DARWIN
    std::uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#endif
#ifdef HAVE_LINUX
    return syscall(SYS_gettid);
#endif
}
} // namespace lo2s
