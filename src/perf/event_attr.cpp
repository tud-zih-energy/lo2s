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

#include <lo2s/build_config.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/pmu.hpp>
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

std::set<Cpu> EventAttr::get_cpu_set_for(EventAttr ev)
{
    std::set<Cpu> cpus = std::set<Cpu>();

    for (const auto& cpu : Topology::instance().cpus())
    {
        try
        {
            EventGuard ev_instance = ev.open(cpu.as_scope(), -1);

            cpus.emplace(cpu);
        }
        catch (const std::system_error& e)
        {
        }
    }

    return cpus;
}

EventAttr::EventAttr(const std::string& name, perf_type_id type, std::uint64_t config,
                     std::uint64_t config1)
: name_(name)
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);

    attr_.type = type;
    attr_.config = config;
    attr_.config1 = config1;
}

PredefinedEventAttr::PredefinedEventAttr(const std::string& name, perf_type_id type,
                                         std::uint64_t config,

                                         std::uint64_t config1)
: EventAttr(name, type, config, config1)
{

    cpus_ = get_cpu_set_for(*this);

    event_is_openable();
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
            throw EventAttr::InvalidEvent("not available!");

            return false;
        }
    }
    return true;
}

void EventAttr::update_availability()
{
    bool proc = false;
    bool system = false;
    try
    {
        EventGuard proc_ev = open(Thread(0));

        if (proc_ev.get_fd() != -1)
        {
            proc = true;
        }
    }
    catch (const std::system_error& e)
    {
    }

    try
    {
        EventGuard sys_ev = open(*supported_cpus().begin());

        if (sys_ev.get_fd() != -1)
        {
            system = true;
        }
    }
    catch (const std::system_error& e)
    {
    }

    if (proc == false && system == false)
    {
        availability_ = Availability::UNAVAILABLE;
    }
    else if (proc == true && system == false)
    {
        availability_ = Availability::PROCESS_MODE;
    }
    else if (proc == false && system == true)
    {
        availability_ = Availability::SYSTEM_MODE;
    }
    else
    {
        availability_ = Availability::UNIVERSAL;
    }
}

RawEventAttr::RawEventAttr(const std::string& ev_name)
: EventAttr(ev_name, PERF_TYPE_RAW, std::stoull(ev_name.substr(1), nullptr, 16), 0)
{
    cpus_ = get_cpu_set_for(*this);

    event_is_openable();
}

static void print_bits(std::ostream& stream, const std::string& name,
                       std::map<uint64_t, std::string> known_bits, uint64_t value)
{
    std::vector<std::string> active_bits;
    for (auto elem : known_bits)
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

    std::map<uint64_t, std::string> read_format = {
        { PERF_FORMAT_TOTAL_TIME_ENABLED, "PERF_FORMAT_TOTAL_TIME_ENABLED" },
        { PERF_FORMAT_TOTAL_TIME_RUNNING, "PERF_FORMAT_TOTAL_TIME_RUNNING" },
        { PERF_FORMAT_ID, "PERF_FORMAT_ID" },
        { PERF_FORMAT_GROUP, "PERF_FORMAT_GROUP" }
    };

    print_bits(stream, "read_format", read_format, event.attr_.read_format);

#ifndef USE_HW_BREAKPOINT_COMPAT
    if (event.attr_.type == PERF_TYPE_BREAKPOINT)
    {
        std::map<uint64_t, std::string> bp_types = { { HW_BREAKPOINT_EMPTY, "HW_BREAKPOINT_EMPTY" },
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

    std::map<uint64_t, std::string> sample_format = {
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

    enum EVENT_DESCRIPTION_REGEX_GROUPS
    {
        ED_WHOLE_MATCH,
        ED_PMU,
        ED_NAME,
    };

    static const std::regex ev_name_regex(R"(([a-z0-9-_]+)[\/:]([a-z0-9-_]+)\/?)");
    std::smatch ev_name_match;

    std::filesystem::path pmu_path;

    if (!std::regex_match(ev_name, ev_name_match, ev_name_regex))
    {
        pmu_path = std::filesystem::path();
    }

    PMU pmu(ev_name_match[1]);
    std::string specific_event = ev_name_match[2];

    Log::debug() << "parsing event description: pmu='" << pmu.name() << "', event='" << name_
                 << "'";

    struct perf_event_attr pmu_attr = pmu.ev_from_name(specific_event);

    attr_.type = pmu.type();
    attr_.config = pmu_attr.config;
    attr_.config1 = pmu_attr.config1;

    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu.name() << "/"
                 << name_ << "/type=" << attr_.type << ",config=" << attr_.config
                 << ",config1=" << attr_.config1 << std::dec << std::noshowbase << "/";

    scale(pmu.scale(specific_event));
    unit(pmu.unit(specific_event));

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

    if (!event_is_openable())
    {
        throw EventAttr::InvalidEvent(
            "Event can not be opened in process- or system-monitoring-mode");
    }
}

EventGuard EventAttr::open(std::variant<Cpu, Thread> location, int cgroup_fd)
{
    return EventGuard(*this, location, -1, cgroup_fd);
}

EventGuard EventAttr::open(ExecutionScope location, int cgroup_fd)
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

EventGuard EventAttr::open_as_group_leader(ExecutionScope location, int cgroup_fd)
{
    attr_.read_format |= PERF_FORMAT_GROUP;
    attr_.sample_type |= PERF_SAMPLE_READ;

    return open(location, cgroup_fd);
}

EventGuard EventGuard::open_child(EventAttr& child, ExecutionScope location, int cgroup_fd)
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

EventGuard::EventGuard(EventAttr& ev, std::variant<Cpu, Thread> location, int group_fd,
                       int cgroup_fd)
: fd_(-1)
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
    Log::trace() << "Succesfully opened perf event! fd: " << fd_;
}

void EventGuard::enable()
{
    if (ioctl(fd_, PERF_EVENT_IOC_ENABLE) == -1)
    {
        throw_errno();
    }
}

void EventGuard::disable()
{
    if (ioctl(fd_, PERF_EVENT_IOC_DISABLE) == -1)
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
