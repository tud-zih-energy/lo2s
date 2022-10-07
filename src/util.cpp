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

std::vector<BlockDevice> get_block_devices()
{
    std::vector<BlockDevice> result;
    std::filesystem::path sys_dev_path("/sys/dev/block");

    const std::regex devname_regex("DEVNAME=(\\S+)");
    const std::regex devtype_regex("DEVTYPE=(\\S+)");
    const std::regex major_regex("MAJOR=(\\S+)");
    const std::regex minor_regex("MINOR=(\\S+)");

    std::smatch devname_match;
    std::smatch devtype_match;
    std::smatch major_match;
    std::smatch minor_match;

    for (const std::filesystem::directory_entry& dir_entry :
         std::filesystem::directory_iterator(sys_dev_path))
    {
        std::string path_str = dir_entry.path().string();
        std::string devname = "unknown device";
        uint32_t major = 0;
        uint32_t minor = 0;
        std::string devtype = "partition";

        std::filesystem::path uevent_path = dir_entry.path() / "uevent";
        std::ifstream uevent_file(uevent_path);

        while (uevent_file.good())
        {
            std::string line;
            uevent_file >> line;
            if (std::regex_match(line, devname_match, devname_regex))
            {
                devname = fmt::format("/dev/{}", devname_match[1].str());
            }
            else if (std::regex_match(line, devtype_match, devtype_regex))
            {
                devtype = devtype_match[1].str();
            }
            else if (std::regex_match(line, major_match, major_regex))
            {
                major = std::stoi(major_match[1].str());
            }
            else if (std::regex_match(line, minor_match, minor_regex))
            {
                minor = std::stoi(minor_match[1].str());
            }
        }

        uint32_t parent_major = 0;
        uint32_t parent_minor = 0;
        if (devtype == "partition")
        {
            std::filesystem::path parent_dev("/sys");

            // Because someone at Linux has a serious glue-sniffing problem these symlinks are
            // relative paths and not absolute. Solution: delete the relative part "../../" from the
            // beginning and make it absolute
            parent_dev =
                parent_dev /
                (std::filesystem::read_symlink(dir_entry.path()).parent_path().string().substr(6));

            std::ifstream parent_uevent_file(parent_dev / "uevent");
            while (parent_uevent_file.good())
            {
                std::string line;
                parent_uevent_file >> line;

                if (std::regex_match(line, major_match, major_regex))
                {
                    parent_major = std::stoi(major_match[1].str());
                }
                else if (std::regex_match(line, minor_match, minor_regex))
                {
                    parent_minor = std::stoi(minor_match[1].str());
                }
            }
        }

        if (devtype == "partition")
        {
            result.push_back(BlockDevice::partition(makedev(major, minor), devname,
                                                    makedev(parent_major, parent_minor)));
        }
        else
        {
            result.push_back(BlockDevice::disk(makedev(major, minor), devname));
        }
    }
    return result;
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
} // namespace lo2s
