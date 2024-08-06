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

#pragma once

#include <cstdint>
#include <set>
#include <type_traits>
#include <variant>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>

extern "C"
{
#include <sys/ioctl.h>

#ifndef USE_HW_BREAKPOINT_COMPAT
#include <linux/hw_breakpoint.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif
}

namespace lo2s
{
namespace perf
{

enum class Availability
{
    UNAVAILABLE,
    SYSTEM_MODE,
    PROCESS_MODE,
    UNIVERSAL
};

class EventGuard;

/**
 * Base class for all Event types
 * contains common attributes
 */
class Event
{
public:
    Event([[maybe_unused]] uint64_t addr, bool enable_on_exec = false);
    Event(std::string name, perf_type_id type, std::uint64_t config, std::uint64_t config1 = 0);
    Event();

    /**
     * returns an opened instance of any Event object
     */
    EventGuard open(std::variant<Cpu, Thread> location, int cgroup_fd = -1);
    EventGuard open(ExecutionScope location, int cgroup_fd = -1);

    /**
     * returns an opened instance of a Event object after formating it as a leader Event
     */
    EventGuard open_as_group_leader(ExecutionScope location, int cgroup_fd = -1);

    const Availability& get_availability() const
    {
        return availability_;
    };

    std::string get_name() const
    {
        return name_;
    }

    std::set<Cpu> get_cpus() const
    {
        return cpus_;
    }

    std::string get_unit() const
    {
        return unit_;
    }

    perf_event_attr& get_attr()
    {
        return attr_;
    }

    double get_scale() const
    {
        return scale_;
    }

    void set_scale(double scale)
    {
        scale_ = scale;
    }

    void set_unit(std::string unit)
    {
        unit_ = unit;
    }

    void set_clock_attrs([[maybe_unused]] const bool& use_clockid,
                         [[maybe_unused]] const clockid_t& clockid)
    {
#ifndef USE_HW_BREAKPOINT_COMPAT
        attr_.use_clockid = use_clockid;
        attr_.clockid = clockid;
#endif
    }

    // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
    // Default behaviour is to wakeup on every event, which is horrible performance wise
    void set_watermark(const size_t& mmap_pages)
    {
        attr_.watermark = 1;
        attr_.wakeup_watermark = static_cast<uint32_t>(0.8 * mmap_pages * sysconf(_SC_PAGESIZE));
    }

    void set_exclude_kernel(const bool& exclude_kernel)
    {
        attr_.exclude_kernel = exclude_kernel;
    }

    void set_sample_period(const int& period);
    void set_sample_freq(const uint64_t& freq);
    void event_attr_update(std::uint64_t value, const std::string& format);

    void parse_pmu_path(const std::string& ev_name);
    void parse_cpus();
    const std::set<Cpu>& supported_cpus() const;

    bool is_valid() const;
    bool event_is_openable();

    bool is_available_in(ExecutionScope scope) const
    {
        // per-process should always work. the counter will just not count if the process is
        // scheduled on a core that is not supprted by that counter
        return scope.is_thread() || cpus_.empty() || cpus_.count(scope.as_cpu());
    }

    bool degrade_precision();

    friend bool operator==(const Event& lhs, const Event& rhs)
    {
        return !memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    friend bool operator<(const Event& lhs, const Event& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    friend bool operator>(const Event& lhs, const Event& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

protected:
    void update_availability();
    void set_common_attrs(bool enable_on_exec);

    struct perf_event_attr attr_;
    double scale_ = 1;
    std::string unit_ = "#";

    std::string name_ = "";
    std::set<Cpu> cpus_;
    Availability availability_ = Availability::UNAVAILABLE;

    std::filesystem::path pmu_path_;
    std::string pmu_name_;
};

/**
 * Contains an event parsed from sysfs
 * @note call on use_sampling_options() after creation to get a valid
 * event, otherwise the availability will be set to UNAVAILABLE
 */
class SysfsEvent : public Event
{
public:
    using Event::Event;
    SysfsEvent(const std::string& ev_name, bool enable_on_exec = false);

    void use_sampling_options(const bool& use_pebs, const bool& sampling, const bool& enable_cct);
};

/**
 * Contains an opened instance of Event.
 * Use any Event.open() method to construct an object
 */
class EventGuard
{
public:
    EventGuard();
    EventGuard(Event& ev, std::variant<Cpu, Thread> location, int group_fd, int cgroup_fd);

    EventGuard(EventGuard&) = delete;
    EventGuard& operator=(const EventGuard&) = delete;

    EventGuard(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
    }

    EventGuard& operator=(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
        return *this;
    }

    /**
     * opens child as a counter of the calling (leader) event
     */
    EventGuard open_child(Event child, ExecutionScope location, int cgroup_fd = -1);

    void enable();
    void disable();

    void set_id(const uint64_t& id)
    {
        ioctl(fd_, PERF_EVENT_IOC_ID, &id);
    }

    void set_output(const EventGuard& other_ev);
    void set_syscall_filter(const std::vector<int64_t>& filter);

    int get_fd() const
    {
        return fd_;
    }

    bool is_valid() const
    {
        return fd_ >= 0;
    };

    template <class T>
    T read()
    {
        static_assert(std::is_pod_v<T> == true);
        T val;

        if (::read(fd_, &val, sizeof(val)) == -1)
        {
            throw_errno();
        }

        return val;
    }

    ~EventGuard()
    {
        close(fd_);
    }

protected:
    int fd_;
    Event ev_;
};

} // namespace perf
} // namespace lo2s
