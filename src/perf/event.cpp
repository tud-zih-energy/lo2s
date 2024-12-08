/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lo2s/perf/event.hpp>
#include <lo2s/perf/event_provider.hpp>

#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/string.hpp>

extern "C"
{
#include <fcntl.h>
#include <sys/ioctl.h>
}

namespace lo2s
{
namespace perf
{

constexpr std::uint64_t operator"" _u64(unsigned long long int lit)
{
    return static_cast<std::uint64_t>(lit);
}

constexpr std::uint64_t bit(int bitnumber)
{
    return static_cast<std::uint64_t>(1_u64 << bitnumber);
}

// helper for visit function
template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename T>
T read_file_or_else(const std::string& filename, T or_else)
{
    T val;
    std::ifstream stream(filename);
    stream >> val;
    if (stream.fail())
    {
        return or_else;
    }
    return val;
}

static std::uint64_t parse_bitmask(const std::string& format)
{
    enum BITMASK_REGEX_GROUPS
    {
        BM_WHOLE_MATCH,
        BM_BEGIN,
        BM_END,
    };

    std::uint64_t mask = 0x0;

    static const std::regex bit_mask_regex(R"((\d+)?(?:-(\d+)))");
    const std::sregex_iterator end;
    for (std::sregex_iterator i = { format.begin(), format.end(), bit_mask_regex }; i != end; ++i)
    {
        const auto& match = *i;
        int start = std::stoi(match[BM_BEGIN]);
        int end = (match[BM_END].length() == 0) ? start : std::stoi(match[BM_END]);

        const auto len = (end + 1) - start;
        if (start < 0 || end > 63 || len > 64)
        {
            throw EventProvider::InvalidEvent("invalid config mask");
        }

        /* Set `len` bits and shift them to where they should start.
         * 4-bit example: format "1-3" produces mask 0b1110.
         *    start := 1, end := 3
         *    len  := 3 + 1 - 1 = 3
         *    bits := bit(3) - 1 = 0b1000 - 1 = 0b0111
         *    mask := 0b0111 << 1 = 0b1110
         * */

        // Shifting by 64 bits causes undefined behaviour, so in this case set
        // all bits by assigning the maximum possible value for std::uint64_t.
        const std::uint64_t bits =
            (len == 64) ? std::numeric_limits<std::uint64_t>::max() : bit(len) - 1;

        mask |= bits << start;
    }
    Log::debug() << std::showbase << std::hex << "config mask: " << format << " = " << mask
                 << std::dec << std::noshowbase;
    return mask;
}

static constexpr std::uint64_t apply_mask(std::uint64_t value, std::uint64_t mask)
{
    std::uint64_t res = 0;
    for (int mask_bit = 0, value_bit = 0; mask_bit < 64; mask_bit++)
    {
        if (mask & bit(mask_bit))
        {
            res |= ((value >> value_bit) & bit(0)) << mask_bit;
            value_bit++;
        }
    }
    return res;
}

Event::Event() : name_("")
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);
    attr_.type = PERF_TYPE_RAW;

    attr_.config = 0;
    attr_.config1 = 0;

    attr_.sample_period = 0;
    attr_.exclude_kernel = 1;
}

Event::Event([[maybe_unused]] uint64_t addr, bool enable_on_exec)
{
    set_common_attrs(enable_on_exec);

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr_.type = PERF_TYPE_BREAKPOINT;
    attr_.bp_type = HW_BREAKPOINT_W;
    attr_.bp_addr = addr;
    attr_.bp_len = HW_BREAKPOINT_LEN_8;
    attr_.wakeup_events = 1;
#else
    attr_.type = PERF_TYPE_HARDWARE;
    attr_.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr_.sample_period = 100000000;
    attr_.task = 1;
#endif
}

Event::Event(const std::string& name, perf_type_id type, std::uint64_t config,
             std::uint64_t config1)
: name_(name)
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);

    attr_.sample_type = PERF_SAMPLE_TIME;
    attr_.type = type;
    attr_.config = config;
    attr_.config1 = config1;

    // Needed when scaling multiplexed events, and recognize activation phases
    attr_.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    try
    {
        parse_pmu_path(name_);
    }
    catch (const EventProvider::InvalidEvent&) // ignore
    {
    }

    parse_cpus();
    update_availability();
}

void Event::parse_pmu_path(const std::string& ev_name)
{
    static const std::regex ev_name_regex(R"(([a-z0-9-_]+)[\/:]([a-z0-9-_]+)\/?)");
    std::smatch ev_name_match;

    if (!std::regex_match(ev_name, ev_name_match, ev_name_regex))
    {
        pmu_path_ = std::filesystem::path();
        throw EventProvider::InvalidEvent("invalid event description format");
    }

    name_ = ev_name_match[2];
    pmu_name_ = ev_name_match[1];
    pmu_path_ = std::filesystem::path("/sys/bus/event_source/devices") / pmu_name_;
}

void Event::set_common_attrs(bool enable_on_exec)
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);
    attr_.type = -1;
    attr_.disabled = 1;

    attr_.sample_period = 1;
    attr_.enable_on_exec = enable_on_exec;

    // Needed when scaling multiplexed events, and recognize activation phases
    attr_.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    attr_.sample_type = PERF_SAMPLE_TIME;
}

void Event::event_attr_update(std::uint64_t value, const std::string& format)
{
    // Parse config terms //

    /* Format:  <term>:<bitmask>
     *
     * We only assign the terms 'config' and 'config1'.
     *
     * */

    static constexpr auto npos = std::string::npos;
    const auto colon = format.find_first_of(':');
    if (colon == npos)
    {
        throw EventProvider::InvalidEvent("invalid format description: missing colon");
    }

    const auto target_config = format.substr(0, colon);
    const auto mask = parse_bitmask(format.substr(colon + 1));

    if (target_config == "config")
    {
        attr_.config |= apply_mask(value, mask);
    }

    if (target_config == "config1")
    {
        attr_.config1 |= apply_mask(value, mask);
    }
}

void Event::parse_cpus()
{
    if (pmu_path_.empty())
    {
        for (const auto& cpu : Topology::instance().cpus())
        {
            try
            {
                EventGuard ev_instance = open(cpu.as_scope(), -1);
                cpus_.emplace(cpu);
            }
            catch (const std::system_error& e)
            {
            }
        }

        return;
    }

    // If the processor is heterogenous, "cpus" contains the cores that support this PMU. If the
    // PMU is an uncore PMU "cpumask" contains the cores that are logically assigned to that
    // PMU. Why there need to be two seperate files instead of one, nobody knows, but simply
    // parse both.
    auto cpuids = parse_list_from_file(pmu_path_ / "cpus");

    if (cpuids.empty())
    {
        cpuids = parse_list_from_file(pmu_path_ / "cpumask");
    }

    std::transform(cpuids.begin(), cpuids.end(), std::inserter(cpus_, cpus_.end()),
                   [](uint32_t cpuid) { return Cpu(cpuid); });
}

void Event::sample_period(const int& period)
{
    Log::debug() << "counter::Reader: sample_period: " << period;
    attr_.sample_period = period;
}

void Event::sample_freq(const uint64_t& freq)
{
    Log::debug() << "counter::Reader: sample_freq: " << freq;
    attr_.sample_freq = freq;
}

const std::set<Cpu>& Event::supported_cpus() const
{
    return cpus_;
}

bool Event::is_valid() const
{
    return (availability_ != Availability::UNAVAILABLE);
}

bool Event::event_is_openable()
{
    update_availability();

    if (!is_valid())
    {
        Log::debug() << "perf event not openable, retrying with exclude_kernel=1";
        attr_.exclude_kernel = 1;
        update_availability();

        if (!is_valid())
        {
            switch (errno)
            {
            case ENOTSUP:
                Log::debug() << "perf event not supported by the running kernel: " << name_;
                break;
            default:
                Log::debug() << "perf event " << name_
                             << " not available: " << std::string(std::strerror(errno));
                break;
            }
            return false;
        }
    }
    return true;
}

void Event::update_availability()
{
    EventGuard proc_ev;
    EventGuard sys_ev;

    try
    {
        proc_ev = open(Thread(0));
    }
    catch (const std::system_error& e)
    {
    }

    try
    {
        sys_ev = open(*supported_cpus().begin());
    }
    catch (const std::system_error& e)
    {
    }

    if (proc_ev.get_fd() == -1)
    {
        if (sys_ev.get_fd() == -1)
            availability_ = Availability::UNAVAILABLE;
        else
            availability_ = Availability::SYSTEM_MODE;
    }
    else
    {
        if (sys_ev.get_fd() == -1)
            availability_ = Availability::PROCESS_MODE;
        else
            availability_ = Availability::UNIVERSAL;
    }
}

bool Event::degrade_precision()
{
    /* reduce exactness of IP can help if the kernel does not support really exact events */
    if (attr_.precise_ip == 0)
    {
        return false;
    }
    else
    {
        attr_.precise_ip--;
        return true;
    }
}

SysfsEvent::SysfsEvent(const std::string& ev_name, bool enable_on_exec) : Event()
{
    set_common_attrs(enable_on_exec);

    // Parse event description //

    /* Event description format:
     *   Name of a Performance Monitoring Unit (PMU) and an event name,
     *   separated by either '/' or ':' (for perf-like syntax);  followed by an
     *   optional separator:
     *
     *     <pmu>/<event_name>[/]
     *   OR
     *     <pmu>:<event_name>[/]
     *
     *   Examples (both specify the same event):
     *
     *     cpu/cache-misses/
     *     cpu:cache-misses
     *
     * */

    enum EVENT_DESCRIPTION_REGEX_GROUPS
    {
        ED_WHOLE_MATCH,
        ED_PMU,
        ED_NAME,
    };

    parse_pmu_path(ev_name);

    Log::debug() << "parsing event description: pmu='" << pmu_name_ << "', event='" << name_ << "'";

    // read PMU type id
    std::underlying_type<perf_type_id>::type type = read_file_or_else(pmu_path_ / "type", 0);
    if (!type)
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown PMU '"s + pmu_name_ + "'");
    }

    attr_.type = static_cast<perf_type_id>(type);
    attr_.config = 0;
    attr_.config1 = 0;

    // Parse event configuration from sysfs //

    // read event configuration
    std::filesystem::path event_path = pmu_path_ / "events" / name_;
    std::string ev_cfg = read_file_or_else(event_path, std::string("0"));
    if (ev_cfg == "0")
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown event '"s + name_ + "' for PMU '"s + pmu_name_ +
                                          "'");
    }

    name_ = ev_name;

    /* Event configuration format:
     *   One or more terms with optional values, separated by ','.  (Taken from
     *   linux/Documentation/ABI/testing/sysfs-bus-event_source-devices-events):
     *
     *     <term>[=<value>][,<term>[=<value>]...]
     *
     *   Example (config for 'cpu/cache-misses' on an Intel Core i5-7200U):
     *
     *     event=0x2e,umask=0x41
     *
     *  */

    enum EVENT_CONFIG_REGEX_GROUPS
    {
        EC_WHOLE_MATCH,
        EC_TERM,
        EC_VALUE,
    };

    static const std::regex kv_regex(R"(([^=,]+)(?:=([^,]+))?)");

    Log::debug() << "parsing event configuration: " << ev_cfg;
    std::smatch kv_match;
    while (std::regex_search(ev_cfg, kv_match, kv_regex))
    {
        static const std::string default_value("0x1");

        const std::string& term = kv_match[EC_TERM];
        const std::string& value =
            (kv_match[EC_VALUE].length() != 0) ? kv_match[EC_VALUE] : default_value;

        std::string format = read_file_or_else(pmu_path_ / "format" / term, std::string("0"));
        if (format == "0")
        {
            throw EventProvider::InvalidEvent("cannot read event format");
        }

        static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                      "May not convert from unsigned long to uint64_t!");

        std::uint64_t val = std::stol(value, nullptr, 0);
        Log::debug() << "parsing config assignment: " << term << " = " << std::hex << std::showbase
                     << val << std::dec << std::noshowbase;
        event_attr_update(val, format);

        ev_cfg = kv_match.suffix();
    }

    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu_name_ << "/"
                 << name_ << "/type=" << attr_.type << ",config=" << attr_.config
                 << ",config1=" << attr_.config1 << std::dec << std::noshowbase << "/";

    scale(read_file_or_else<double>(event_path.replace_extension(".scale"), 1.0));
    unit(read_file_or_else<std::string>(event_path.replace_extension(".unit"), "#"));

    if (!event_is_openable())
    {
        throw EventProvider::InvalidEvent(
            "Event can not be opened in process- or system-monitoring-mode");
    }
}

void SysfsEvent::use_sampling_options(const bool& use_pebs, const bool& sampling,
                                      const bool& enable_cct)
{
    if (use_pebs)
    {
        attr_.use_clockid = 0;
    }

    if (sampling)
    {
        Log::debug() << "using sampling event \'" << name_ << "\', period: " << attr_.sample_period;

        attr_.mmap = 1;
    }
    else
    {
        // Set up a dummy event for recording calling context enter/leaves only
        attr_.type = PERF_TYPE_SOFTWARE;
        attr_.config = PERF_COUNT_SW_DUMMY;
    }

    attr_.sample_id_all = 1;
    // Generate PERF_RECORD_COMM events to trace changes to the command
    // name of a task.  This is used to write a meaningful name for any
    // traced thread to the archive.
    attr_.comm = 1;
    attr_.context_switch = 1;

    // TODO see if we can remove remove tid
    attr_.sample_type |= PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_CPU;
    if (enable_cct)
    {
        attr_.sample_type |= PERF_SAMPLE_CALLCHAIN;
    }

    attr_.precise_ip = 3;

    // make event available if possible
    update_availability();
}

EventGuard Event::open(std::variant<Cpu, Thread> location, int cgroup_fd)
{
    return EventGuard(*this, location, -1, cgroup_fd);
}

EventGuard Event::open(ExecutionScope location, int cgroup_fd)
{
    if (location.is_cpu())
    {
        return EventGuard(*this, location.as_cpu(), -1, cgroup_fd);
    }
    else
    {
        return EventGuard(*this, location.as_thread(), -1, cgroup_fd);
    }
}

EventGuard Event::open_as_group_leader(ExecutionScope location, int cgroup_fd)
{
    attr_.read_format |= PERF_FORMAT_GROUP;
    attr_.sample_type |= PERF_SAMPLE_READ;

    return open(location, cgroup_fd);
}

EventGuard EventGuard::open_child(Event child, ExecutionScope location, int cgroup_fd)
{
    if (location.is_cpu())
    {
        return EventGuard(child, location.as_cpu(), fd_, cgroup_fd);
    }
    else
    {
        return EventGuard(child, location.as_thread(), fd_, cgroup_fd);
    }
}

EventGuard::EventGuard() : fd_(-1)
{
}

EventGuard::EventGuard(Event& ev, std::variant<Cpu, Thread> location, int group_fd, int cgroup_fd)
: ev_(ev)
{
    // can be deleted when scope gets replaced
    ExecutionScope scope;
    std::visit(overloaded{ [&](Cpu cpu) { scope = cpu.as_scope(); },
                           [&](Thread thread) { scope = thread.as_scope(); } },
               location);

    fd_ = perf_event_open(&ev_.mut_attr(), scope, group_fd, 0, cgroup_fd);

    if (fd_ < 0)
    {
        throw_errno();
    }

    if (fcntl(fd_, F_SETFL, O_NONBLOCK))
    {
        Log::error() << errno;
        throw_errno();
    }
}

void EventGuard::enable()
{
    auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
    if (ret == -1)
    {
        throw_errno();
    }
}

void EventGuard::disable()
{
    auto ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE);
    if (ret == -1)
    {
        throw_errno();
    }
}

void EventGuard::set_output(const EventGuard& other_ev)
{
    if (ioctl(fd_, PERF_EVENT_IOC_SET_OUTPUT, other_ev.get_fd()) == -1)
    {
        throw_errno();
    }
}

void EventGuard::set_syscall_filter(const std::vector<int64_t>& syscall_filter)
{
    if (syscall_filter.empty())
    {
        return;
    }

    std::vector<std::string> names;
    std::transform(syscall_filter.cbegin(), syscall_filter.end(), std::back_inserter(names),
                   [](const auto& elem) { return fmt::format("id == {}", elem); });
    std::string filter = fmt::format("{}", fmt::join(names, "||"));

    if (ioctl(fd_, PERF_EVENT_IOC_SET_FILTER, filter.c_str()) == -1)
    {
        throw_errno();
    }
}

} // namespace perf
} // namespace lo2s
