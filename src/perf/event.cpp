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

// helper for visit function
template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

PerfEvent::PerfEvent() : name_("")
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);
    attr_.type = PERF_TYPE_RAW;

    attr_.config = 0;
    attr_.config1 = 0;

    attr_.sample_period = 0;
    attr_.exclude_kernel = 1;
}

PerfEvent::PerfEvent(const std::string& ev_name, bool enable_on_exec) : name_(ev_name)
{
    set_common_attrs(enable_on_exec);

    PerfEvent other = EventProvider::instance().get_event_by_name(ev_name);
    attr_.type = static_cast<perf_type_id>(other.get_attr().type);
    attr_.config = other.get_attr().config;
    attr_.config1 = other.get_attr().config1;
    cpus_ = other.get_cpus();

    set_availability();
}

PerfEvent::PerfEvent([[maybe_unused]] uint64_t addr, bool enable_on_exec)
{
    set_common_attrs(enable_on_exec);
    attr_.exclude_kernel = 1;

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr_.type = PERF_TYPE_BREAKPOINT;
    attr_.bp_type = HW_BREAKPOINT_W;
    attr_.bp_len = HW_BREAKPOINT_LEN_8;
    attr_.wakeup_events = 1;
    attr_.bp_addr = addr;
#else
    attr_.type = PERF_TYPE_HARDWARE;
    attr_.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr_.sample_period = 100000000;
    attr_.task = 1;
#endif
}

PerfEvent::PerfEvent(std::string name, perf_type_id type, std::uint64_t config,
                     std::uint64_t config1, std::set<Cpu> cpus)
: name_(name), cpus_(cpus)
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);
    attr_.type = -1;

    attr_.sample_type = PERF_SAMPLE_TIME;
    attr_.type = type;
    attr_.config = config;
    attr_.config1 = config1;

    set_availability();
}

void PerfEvent::set_common_attrs(bool enable_on_exec)
{
    memset(&attr_, 0, sizeof(attr_));
    attr_.size = sizeof(attr_);
    attr_.type = -1;
    attr_.disabled = 1;

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr_.use_clockid = config().use_clockid;
    attr_.clockid = config().clockid;
#endif

    // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
    // Default behaviour is to wakeup on every event, which is horrible performance wise
    attr_.watermark = 1;
    attr_.wakeup_watermark =
        static_cast<uint32_t>(0.8 * config().mmap_pages * sysconf(_SC_PAGESIZE));

    attr_.exclude_kernel = config().exclude_kernel;
    attr_.sample_period = 1;
    attr_.enable_on_exec = enable_on_exec;

    // Needed when scaling multiplexed events, and recognize activation phases
    attr_.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    attr_.sample_type = PERF_SAMPLE_TIME;
}

void PerfEvent::set_sample_period(const int& period)
{
    Log::debug() << "counter::Reader: sample_period: " << period;
    attr_.sample_period = period;
}

void PerfEvent::set_sample_freq()
{
    attr_.freq = config().metric_use_frequency;
    if (attr_.freq)
    {
        Log::debug() << "counter::Reader: sample_freq: " << config().metric_frequency;
        attr_.sample_freq = config().metric_frequency;
    }
    else
    {
        set_sample_period(config().metric_count);
    }
}

const std::set<Cpu>& PerfEvent::supported_cpus() const
{
    if (cpus_.empty())
    {
        return Topology::instance().cpus();
    }
    return cpus_;
}

bool PerfEvent::is_valid() const
{
    return (availability_ != Availability::UNAVAILABLE);
}

void PerfEvent::set_availability()
{
    PerfEventGuard proc_ev;
    PerfEventGuard sys_ev;

    try
    {
        proc_ev = open(Thread(0), -1);
    }
    catch (const std::system_error& e)
    {
    }

    try
    {
        sys_ev = open(*supported_cpus().begin(), -1);
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

bool PerfEvent::degrade_precision()
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

void SysfsEvent::as_sample()
{
    if (config().use_pebs)
    {
        attr_.use_clockid = 0;
    }

    attr_.sample_period = config().sampling_period;

    if (config().sampling)
    {
        Log::debug() << "using sampling event \'" << config().sampling_event
                     << "\', period: " << config().sampling_period;

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
    if (config().enable_cct)
    {
        attr_.sample_type |= PERF_SAMPLE_CALLCHAIN;
    }

    attr_.precise_ip = 3;

    // make event available if possible
    set_availability();
}

PerfEventGuard PerfEvent::open(std::variant<Cpu, Thread> location, int cgroup_fd)
{
    return PerfEventGuard(*this, location, -1, cgroup_fd);
}

PerfEventGuard PerfEvent::open(ExecutionScope location, int cgroup_fd)
{
    if (location.is_cpu())
    {
        return PerfEventGuard(*this, location.as_cpu(), -1, cgroup_fd);
    }
    else
    {
        return PerfEventGuard(*this, location.as_thread(), -1, cgroup_fd);
    }
}

PerfEventGuard PerfEvent::open_as_group_leader(std::variant<Cpu, Thread> location, int cgroup_fd)
{
    attr_.read_format |= PERF_FORMAT_GROUP;
    attr_.sample_type |= PERF_SAMPLE_READ;

    return open(location, cgroup_fd);
}

PerfEventGuard PerfEvent::open_as_group_leader(ExecutionScope location, int cgroup_fd)
{
    attr_.read_format |= PERF_FORMAT_GROUP;
    attr_.sample_type |= PERF_SAMPLE_READ;

    return open(location, cgroup_fd);
}

PerfEventGuard PerfEventGuard::open_child(PerfEvent child, std::variant<Cpu, Thread> location,
                                          int cgroup_fd)
{
    return PerfEventGuard(child, location, fd_, cgroup_fd);
}

PerfEventGuard PerfEventGuard::open_child(PerfEvent child, ExecutionScope location, int cgroup_fd)
{
    if (location.is_cpu())
    {
        return PerfEventGuard(child, location.as_cpu(), fd_, cgroup_fd);
    }
    else
    {
        return PerfEventGuard(child, location.as_thread(), fd_, cgroup_fd);
    }
}

PerfEventGuard::PerfEventGuard() : fd_(-1)
{
}

PerfEventGuard::PerfEventGuard(PerfEvent& ev, std::variant<Cpu, Thread> location, int group_fd,
                               int cgroup_fd)
: ev_(ev)
{
    // can be deleted when scope gets replaced
    ExecutionScope scope;
    std::visit(overloaded{ [&](Cpu cpu) { scope = cpu.as_scope(); },
                           [&](Thread thread) { scope = thread.as_scope(); } },
               location);

    fd_ = perf_event_open(&ev_.get_attr(), scope, group_fd, 0, cgroup_fd);

    // other error handling
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

void PerfEventGuard::enable()
{
    auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
    if (ret == -1)
    {
        throw_errno();
    }
}

void PerfEventGuard::disable()
{
    auto ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE);
    if (ret == -1)
    {
        throw_errno();
    }
}

void PerfEventGuard::set_output(const PerfEventGuard& other_ev)
{
    if (ioctl(fd_, PERF_EVENT_IOC_SET_OUTPUT, other_ev.get_fd()) == -1)
    {
        throw_errno();
    }
}

void PerfEventGuard::set_syscall_filter()
{
    std::vector<std::string> names;
    std::transform(config().syscall_filter.cbegin(), config().syscall_filter.end(),
                   std::back_inserter(names),
                   [](const auto& elem) { return fmt::format("id == {}", elem); });
    std::string filter = fmt::format("{}", fmt::join(names, "||"));

    if (ioctl(fd_, PERF_EVENT_IOC_SET_FILTER, filter.c_str()) == -1)
    {
        throw_errno();
    }
}

} // namespace perf
} // namespace lo2s
