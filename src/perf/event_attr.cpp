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

#include <lo2s/perf/event_attr.hpp>

#include <lo2s/build_config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/util.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <regex>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <unistd.h>

extern "C"
{
#include <fcntl.h>
#include <sys/ioctl.h>
}

namespace lo2s::perf
{
namespace
{
std::set<Cpu> get_cpu_set_for(const EventAttr& ev)
{
    std::set<Cpu> cpus = std::set<Cpu>();

    for (const auto& cpu : Topology::instance().cpus())
    {
        if (ev.can_open(cpu.as_scope()))
        {
            cpus.emplace(cpu);
        }
    }

    return cpus;
}

template <typename T>
std::optional<T> try_read_file(const std::string& filename)
{
    T val;
    std::ifstream stream(filename);
    stream >> val;
    if (stream.fail())
    {
        return {};
    }
    return val;
}

std::uint64_t parse_bitmask(const std::string& format)
{
    constexpr int BM_BEGIN = 1;
    constexpr int BM_END = 2;

    std::uint64_t mask = 0x0;

    static const std::regex bit_mask_regex(R"((\d+)?(?:-(\d+)))");
    const std::sregex_iterator end;
    for (std::sregex_iterator i = { format.begin(), format.end(), bit_mask_regex }; i != end; ++i)
    {
        const auto& match = *i;
        int const start = std::stoi(match[BM_BEGIN]);
        int const end = (match[BM_END].length() == 0) ? start : std::stoi(match[BM_END]);

        const auto len = (end + 1) - start;
        if (start < 0 || end > 64 - 1 || len > 64)
        {
            throw EventAttr::InvalidEvent("invalid config mask");
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
            (len == 64) ? std::numeric_limits<std::uint64_t>::max() : (1ULL << len) - 1;

        mask |= bits << start;
    }
    Log::debug() << std::showbase << std::hex << "config mask: " << format << " = " << mask
                 << std::dec << std::noshowbase;
    return mask;
}

constexpr std::uint64_t apply_mask(std::uint64_t value, std::uint64_t mask)
{
    std::uint64_t res = 0;
    for (int mask_bit = 0, value_bit = 0; mask_bit < 64; mask_bit++)
    {
        if (mask & (1ULL << mask_bit))
        {
            res |= ((value >> value_bit) & (1ULL << 0)) << mask_bit;
            value_bit++;
        }
    }
    return res;
}
} // namespace

EventAttr::EventAttr(std::string name, perf_type_id type, std::uint64_t config,
                     std::uint64_t config1)
: name_(std::move(name))
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);

    attr_.type = type;
    attr_.config = config;
    attr_.config1 = config1;
}

PredefinedEventAttr::PredefinedEventAttr(const std::string& name, perf_type_id type,
                                         std::uint64_t config,

                                         std::uint64_t config1, const std::set<Cpu>& cpus)
: EventAttr(name, type, config, config1)
{
    if (cpus.empty())
    {
        cpus_ = get_cpu_set_for(*this);
    }
    else
    {
        cpus_ = cpus;
    }

    event_is_openable();
}

void EventAttr::event_attr_update(std::uint64_t value, const std::string& format)
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
        throw EventAttr::InvalidEvent("invalid format description: missing colon");
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

void EventAttr::sample_period(int period)
{
    Log::debug() << "counter::Reader: sample_period: " << period;
    attr_.freq = false;
    attr_.sample_period = period;
}

void EventAttr::sample_freq(uint64_t freq)
{
    Log::debug() << "counter::Reader: sample_freq: " << freq;
    attr_.freq = true;
    attr_.sample_freq = freq;
}

const std::set<Cpu>& EventAttr::supported_cpus() const
{
    return cpus_;
}

bool EventAttr::event_is_openable()
{
    update_availability();

    if (availability_ == Availability::UNAVAILABLE)
    {
        Log::debug() << "perf event not openable, retrying with exclude_kernel=1";
        attr_.exclude_kernel = 1;
        update_availability();

        if (availability_ == Availability::UNAVAILABLE)
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

void EventAttr::update_availability()
{
    bool const proc = can_open(Thread(0).as_scope());
    bool const system = can_open(supported_cpus().begin()->as_scope());

    if (!proc && !system)
    {
        availability_ = Availability::UNAVAILABLE;
    }
    else if (proc && !system)
    {
        availability_ = Availability::PROCESS_MODE;
    }
    else if (!proc && system)
    {
        availability_ = Availability::SYSTEM_MODE;
    }
    else
    {
        availability_ = Availability::UNIVERSAL;
    }
}

constexpr int BASE16 = 16;

RawEventAttr::RawEventAttr(const std::string& ev_name)
: EventAttr(ev_name, PERF_TYPE_RAW, std::stoull(ev_name.substr(1), nullptr, BASE16), 0)
{
    cpus_ = get_cpu_set_for(*this);

    event_is_openable();
}

namespace
{
void print_bits(std::ostream& stream, const std::string& name,
                const std::map<uint64_t, std::string>& known_bits, uint64_t value)
{
    std::vector<std::string> active_bits;
    for (const auto& elem : known_bits)
    {
        if (value & elem.first)
        {
            active_bits.push_back(elem.second);
            value &= ~elem.first;
        }
    }

    if (value != 0)
    {
        stream << "Unknown perf_event_attr." << name << " bits: " << std::hex << value << std::dec;
    }

    std::string active_bits_str;
    if (!active_bits.empty())
    {
        auto it = active_bits.begin();
        for (; it != --active_bits.end(); it++)
        {
            active_bits_str += *it;
            active_bits_str += " | ";
        }
        active_bits_str += *it;
    }
    stream << "\t" << name << ": " << active_bits_str << "\n";
}
} // namespace

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::ostream& operator<<(std::ostream& stream, const EventAttr& event)
{
    stream << "{\n";
    switch (event.attr_.type)
    {
    case PERF_TYPE_HARDWARE:
        stream << "\ttype: PERF_TYPE_HARDWARE\n";
        break;
    case PERF_TYPE_SOFTWARE:
        stream << "\ttype: PERF_TYPE_SOFTWARE\n";
        break;
    case PERF_TYPE_TRACEPOINT:
        stream << "\ttype: PERF_TYPE_TRACEPOINT\n";
        break;
    case PERF_TYPE_HW_CACHE:
        stream << "\ttype: PERF_TYPE_HW_CACHE\n";
        break;
    case PERF_TYPE_RAW:
        stream << "\ttype: PERF_TYPE_RAW\n";
        break;
    case PERF_TYPE_BREAKPOINT:
        stream << "\ttype: PERF_TYPE_BREAKPOINT\n";
        break;
    default:
        stream << "\ttype: " << std::hex << event.attr_.type << std::dec << "\n";
    }
    stream << "\tsize: " << event.attr_.size << "\n";
    stream << std::hex << "\tconfig: 0x" << event.attr_.config << std::dec << "\n";
    stream << std::hex << "\tconfig1: 0x" << event.attr_.config1 << std::dec << "\n";
    stream << std::hex << "\tconfig2: 0x" << event.attr_.config2 << std::dec << "\n";
    stream << "\tprecise_ip: " << event.attr_.precise_ip << "/3\n";

    std::map<uint64_t, std::string> const read_format = {
        { PERF_FORMAT_TOTAL_TIME_ENABLED, "PERF_FORMAT_TOTAL_TIME_ENABLED" },
        { PERF_FORMAT_TOTAL_TIME_RUNNING, "PERF_FORMAT_TOTAL_TIME_RUNNING" },
        { PERF_FORMAT_ID, "PERF_FORMAT_ID" },
        { PERF_FORMAT_GROUP, "PERF_FORMAT_GROUP" }
    };

    print_bits(stream, "read_format", read_format, event.attr_.read_format);

#ifndef USE_HW_BREAKPOINT_COMPAT
    if (event.attr_.type == PERF_TYPE_BREAKPOINT)
    {
        std::map<uint64_t, std::string> const bp_types = { { HW_BREAKPOINT_EMPTY,
                                                             "HW_BREAKPOINT_EMPTY" },
                                                           { HW_BREAKPOINT_W, "HW_BREAKPOINT_W" },
                                                           { HW_BREAKPOINT_R, "HW_BREAKPOINT_R" },
                                                           { HW_BREAKPOINT_RW, "HW_BREAKPOINT_RW" },
                                                           { HW_BREAKPOINT_X, "HW_BREAKPOINT_X" } };

        print_bits(stream, "bp_type", bp_types, event.attr_.bp_type);
        stream << "\tbp_addr: 0x" << std::hex << event.attr_.bp_addr << std::dec << "\n";
        switch (event.attr_.bp_len)
        {
        case HW_BREAKPOINT_LEN_1:
            stream << "\tbp_len: HW_BREAKPOINT_LEN_1\n";
            break;
        case HW_BREAKPOINT_LEN_2:
            stream << "\tbp_len: HW_BREAKPOINT_LEN_2\n";
            break;
        case HW_BREAKPOINT_LEN_4:
            stream << "\tbp_len: HW_BREAKPOINT_LEN_4\n";
            break;
        case HW_BREAKPOINT_LEN_8:
            stream << "\tbp_len: HW_BREAKPOINT_LEN_8\n";
            break;
        default:
            stream << "\tbp_len: " << event.attr_.bp_len << "\n";
        }
    }
#endif
    if (event.attr_.freq)
    {
        stream << "\tsample_freq: " << event.attr_.sample_freq << " Hz\n";
    }
    else
    {
        stream << "\tsample_period: " << event.attr_.sample_period << " event(s)\n";
    }

    std::map<uint64_t, std::string> const sample_format = {
        { PERF_SAMPLE_CALLCHAIN, "PERF_SAMPLE_CALLCHAIN" },
        { PERF_SAMPLE_READ, "PERF_SAMPLE_READ" },
        { PERF_SAMPLE_IP, "PERF_SAMPLE_IP" },
        { PERF_SAMPLE_TID, "PERF_SAMPLE_TID" },
        { PERF_SAMPLE_CPU, "PERF_SAMPLE_CPU" },
        { PERF_SAMPLE_TIME, "PERF_SAMPLE_TIME" },
        { PERF_SAMPLE_RAW, "PERF_SAMPLE_RAW" },
        { PERF_SAMPLE_IDENTIFIER, "PERF_SAMPLE_IDENTIFIER" }
    };

    print_bits(stream, "sample_type", sample_format, event.attr_.sample_type);

    stream << "\tFLAGS: \n";
    if (event.attr_.disabled)
    {
        stream << "\t\t- disabled\n";
    }
    if (event.attr_.pinned)
    {
        stream << "\t\t- pinned\n";
    }
    if (event.attr_.exclusive)
    {
        stream << "\t\t- exclusive\n";
    }
    if (event.attr_.exclude_kernel)
    {
        stream << "\t\t- exclude_kernel\n";
    }
    if (event.attr_.exclude_user)
    {
        stream << "\t\t- exclude_user\n";
    }
    if (event.attr_.exclude_hv)
    {
        stream << "\t\t- exclude_hv\n";
    }
    if (event.attr_.exclude_idle)
    {
        stream << "\t\t- exclude_idle\n";
    }
    if (event.attr_.mmap)
    {
        stream << "\t\t- mmap\n";
    }
    if (event.attr_.inherit_stat)
    {
        stream << "\t\t- inherit_stat\n";
    }
    if (event.attr_.enable_on_exec)
    {
        stream << "\t\t- enable_on_exec\n";
    }
    if (event.attr_.task)
    {
        stream << "\t\t- task\n";
    }
    if (event.attr_.watermark)
    {
        stream << "\t\t- wakeup_watermark (" << event.attr_.wakeup_watermark << " bytes)"
               << "\n";
    }
    else
    {

        stream << "\t\t- wakeup_events (" << event.attr_.wakeup_events << " events)\n";
    }
    if (event.attr_.mmap_data)
    {
        stream << "\t\t- mmap_data\n";
    }
    if (event.attr_.sample_id_all)
    {
        stream << "\t\t- sample_id_all\n";
    }
    if (event.attr_.mmap2)
    {
        stream << "\t\t- mmap2\n";
    }
    if (event.attr_.context_switch)
    {
        stream << "\t\t- context_switch\n";
    }
    if (event.attr_.comm_exec)
    {
        stream << "\t\t- comm_exec\n";
    }
    if (event.attr_.comm)
    {
        stream << "\t\t- comm\n";
    }

    if (event.attr_.comm_exec)
    {
        stream << "\t\t- comm_exec\n";
    }
    if (event.attr_.use_clockid)
    {
        std::string clock;
        switch (event.attr_.clockid)
        {
        case CLOCK_MONOTONIC:
            clock = "monotonic";
            break;
        case CLOCK_MONOTONIC_RAW:
            clock = "monotonic-raw";
            break;
        case CLOCK_REALTIME:
            clock = "realtime";
            break;
        case CLOCK_BOOTTIME:
            clock = "boottime";
            break;
        case CLOCK_TAI:
            clock = "tai";
            break;
        default:
            stream << "Unknown clockid: " << event.attr_.clockid << "\n";
            clock = "???";
            break;
        }
        stream << "\t\t- clockid (" << clock << ")\n";
    }

    stream << "}\n";
    return stream;
}

// NOLINTEND(readability-function-cognitive-complexity)

SysfsEventAttr::SysfsEventAttr(const std::string& ev_name)
: EventAttr(ev_name, static_cast<perf_type_id>(0), 0)
{
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

    static const std::regex ev_name_regex(R"(([a-z0-9-_]+)[\/:]([a-z0-9-_]+)\/?)");
    std::smatch ev_name_match;

    std::filesystem::path pmu_path;

    if (!std::regex_match(ev_name, ev_name_match, ev_name_regex))
    {
        pmu_path = std::filesystem::path();
    }

    name_ = ev_name_match[2];
    std::string const pmu_name = ev_name_match[1];
    pmu_path = std::filesystem::path("/sys/bus/event_source/devices") / pmu_name;

    Log::debug() << "parsing event description: pmu='" << pmu_name << "', event='" << name_ << "'";

    // read PMU type id
    auto type = try_read_file<std::underlying_type_t<perf_type_id>>(pmu_path / "type");

    if (!type.has_value())
    {
        using namespace std::string_literals;
        throw EventAttr::InvalidEvent("unknown PMU '"s + pmu_name + "'");
    }

    attr_.type = static_cast<perf_type_id>(type.value());
    attr_.config = 0;
    attr_.config1 = 0;

    // Parse event configuration from sysfs //

    // read event configuration
    std::filesystem::path event_path = pmu_path / "events" / name_;
    auto ev_cfg = try_read_file<std::string>(event_path);
    if (!ev_cfg.has_value())
    {
        using namespace std::string_literals;
        throw EventAttr::InvalidEvent("unknown event '"s + name_ + "' for PMU '"s + pmu_name + "'");
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

    constexpr int EC_TERM = 1;
    constexpr int EC_VALUE = 2;

    // If the processor is heterogenous, "cpus" contains the cores that support this PMU. If the
    // PMU is an uncore PMU "cpumask" contains the cores that are logically assigned to that
    // PMU. Why there need to be two seperate files instead of one, nobody knows, but simply
    // parse both.
    auto cpuids = parse_list_from_file(pmu_path / "cpus");

    if (cpuids.empty())
    {
        cpuids = parse_list_from_file(pmu_path / "cpumask");
    }

    if (cpuids.empty())
    {
        cpus_ = get_cpu_set_for(*this);
    }
    else
    {
        std::transform(cpuids.begin(), cpuids.end(), std::inserter(cpus_, cpus_.end()),
                       [](uint32_t cpuid) { return Cpu(cpuid); });
    }

    static const std::regex kv_regex(R"(([^=,]+)(?:=([^,]+))?)");

    Log::debug() << "parsing event configuration: " << ev_cfg.value();
    std::smatch kv_match;
    while (std::regex_search(ev_cfg.value(), kv_match, kv_regex))
    {
        static const std::string default_value("0x1");

        const std::string& term = kv_match[EC_TERM];
        const std::string& value =
            (kv_match[EC_VALUE].length() != 0) ? kv_match[EC_VALUE] : default_value;

        auto format = try_read_file<std::string>(pmu_path / "format" / term);
        if (!format.has_value())
        {
            throw EventAttr::InvalidEvent("cannot read event format");
        }

        static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                      "May not convert from unsigned long to uint64_t!");

        std::uint64_t const val = std::stol(value, nullptr, 0);
        Log::debug() << "parsing config assignment: " << term << " = " << std::hex << std::showbase
                     << val << std::dec << std::noshowbase;
        event_attr_update(val, format.value());

        ev_cfg = kv_match.suffix();
    }

    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu_name << "/"
                 << name_ << "/type=" << attr_.type << ",config=" << attr_.config
                 << ",config1=" << attr_.config1 << std::dec << std::noshowbase << "/";

    scale(try_read_file<double>(event_path.replace_extension(".scale")).value_or(1.0));
    unit(try_read_file<std::string>(event_path.replace_extension(".unit")).value_or("#"));

    if (!event_is_openable())
    {
        throw EventAttr::InvalidEvent(
            "Event can not be opened in process- or system-monitoring-mode");
    }
}

EventGuard EventAttr::open(std::variant<Cpu, Thread> location, int cgroup_fd)
{
    return { *this, location, -1, cgroup_fd };
}

EventGuard EventAttr::open(ExecutionScope location, int cgroup_fd)
{
    if (location.is_cpu())
    {
        return { *this, location.as_cpu(), -1, cgroup_fd };
    }

    return { *this, location.as_thread(), -1, cgroup_fd };
}

bool EventAttr::can_open(ExecutionScope location) const
{
    int const fd = perf_event_open(&attr(), location, -1, 0, -1);

    if (fd < 0)
    {
        return false;
    }

    close(fd);
    return true;
}

EventGuard EventAttr::open_as_group_leader(ExecutionScope location, int cgroup_fd)
{
    attr_.read_format |= PERF_FORMAT_GROUP;
    attr_.sample_type |= PERF_SAMPLE_READ;

    return open(location, cgroup_fd);
}

EventGuard EventGuard::open_child(EventAttr& child, ExecutionScope location, int cgroup_fd) const
{
    if (location.is_cpu())
    {
        return { child, location.as_cpu(), fd_, cgroup_fd };
    }
    return { child, location.as_thread(), fd_, cgroup_fd };
}

EventGuard::EventGuard(EventAttr& ev, std::variant<Cpu, Thread> location, int group_fd,
                       int cgroup_fd)
{
    ExecutionScope scope;
    std::visit([&scope](auto loc) { scope = loc.as_scope(); }, location);

    Log::trace() << "Opening perf event: " << ev.name() << "[" << scope.name()
                 << ", group fd: " << group_fd << ", cgroup fd: " << cgroup_fd << "]";
    Log::trace() << ev;
    fd_ = perf_event_open(&ev.attr(), scope, group_fd, 0, cgroup_fd);

    if (fd_ < 0)
    {
        Log::trace() << "Couldn't open event!: " << strerror(errno);
        throw_errno();
    }

    if (fcntl(fd_, F_SETFL, O_NONBLOCK))
    {
        Log::trace() << "Couldn't set event nonblocking: " << strerror(errno);
        throw_errno();
    }
    Log::trace() << "SuccesfULLy opened perf event! fd: " << fd_;
}

void EventGuard::enable() const
{
    if (ioctl(fd_, PERF_EVENT_IOC_ENABLE) == -1)
    {
        throw_errno();
    }
}

void EventGuard::disable() const
{
    if (ioctl(fd_, PERF_EVENT_IOC_DISABLE) == -1)
    {
        throw_errno();
    }
}

void EventGuard::set_output(const EventGuard& other_ev) const
{
    if (ioctl(fd_, PERF_EVENT_IOC_SET_OUTPUT, other_ev.get_fd()) == -1)
    {
        throw_errno();
    }
}

void EventGuard::set_syscall_filter(const std::vector<int64_t>& syscall_filter) const
{
    if (syscall_filter.empty())
    {
        return;
    }

    std::vector<std::string> names;
    std::transform(syscall_filter.cbegin(), syscall_filter.end(), std::back_inserter(names),
                   [](const auto& elem) { return fmt::format("id == {}", elem); });
    std::string const filter = fmt::format("{}", fmt::join(names, "||"));

    if (ioctl(fd_, PERF_EVENT_IOC_SET_FILTER, filter.c_str()) == -1)
    {
        throw_errno();
    }
}

} // namespace lo2s::perf
