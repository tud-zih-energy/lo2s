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

#ifndef USE_HW_BREAKPOINT_COMPAT
extern "C"
{
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
}
#else
extern "C"
{
#include <sys/types.h>
#include <sys/wait.h>
}
#endif

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

class PerfEventGuard;

/**
 * Base class for all Event types
 * contains common attributes
 */
class PerfEvent
{
public:
    PerfEvent(const std::string& ev_name, bool enable_on_exec = false);
    PerfEvent([[maybe_unused]] uint64_t addr, bool enable_on_exec = false);
    PerfEvent(std::string name, perf_type_id type, std::uint64_t config, std::uint64_t config1 = 0,
              std::set<Cpu> cpus = std::set<Cpu>());
    PerfEvent();

    /**
     * returns an opened instance of any PerfEvent object
     */
    PerfEventGuard open(std::variant<Cpu, Thread> location, int cgroup_fd = config().cgroup_fd);
    PerfEventGuard open(ExecutionScope location, int cgroup_fd = config().cgroup_fd);

    /**
     * returns an opened instance of a PerfEvent object after formating it as a leader Event
     */
    PerfEventGuard open_as_group_leader(std::variant<Cpu, Thread> location,
                                        int cgroup_fd = config().cgroup_fd);
    PerfEventGuard open_as_group_leader(ExecutionScope location,
                                        int cgroup_fd = config().cgroup_fd);

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

    void set_sample_period(const int& period);
    void set_sample_freq();
    void set_availability();

    const std::set<Cpu>& supported_cpus() const;
    bool is_valid() const;

    bool is_available_in(ExecutionScope scope) const
    {
        // per-process should always work. the counter will just not count if the process is
        // scheduled on a core that is not supprted by that counter
        return scope.is_thread() || cpus_.empty() || cpus_.count(scope.as_cpu());
    }

    bool degrade_precision();

    friend bool operator==(const PerfEvent& lhs, const PerfEvent& rhs)
    {
        return !memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    friend bool operator<(const PerfEvent& lhs, const PerfEvent& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    friend bool operator>(const PerfEvent& lhs, const PerfEvent& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

protected:
    void set_common_attrs(bool enable_on_exec);

    struct perf_event_attr attr_;
    double scale_ = 1;
    std::string unit_ = "#";

    std::string name_ = "";
    std::set<Cpu> cpus_;
    Availability availability_ = Availability::UNAVAILABLE;
};

/**
 * Contains an event parsed from sysfs
 * @note call on as_sample() after creation to get a valid
 * event, otherwise the availability will be set to UNAVAILABLE
 */
class SysfsEvent : public PerfEvent
{
public:
    using PerfEvent::PerfEvent;

    void as_sample();
};

/**
 * Contains an opened instance of PerfEvent.
 * Use any PerfEvent.open() method to construct an object
 */
class PerfEventGuard
{
public:
    PerfEventGuard();
    PerfEventGuard(PerfEvent& ev, std::variant<Cpu, Thread> location, int group_fd, int cgroup_fd);

    PerfEventGuard(PerfEventGuard&) = delete;
    PerfEventGuard& operator=(const PerfEventGuard&) = delete;

    PerfEventGuard(PerfEventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
    }

    PerfEventGuard& operator=(PerfEventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
        return *this;
    }

    /**
     * opens child as a counter of the calling (leader) event
     */
    PerfEventGuard open_child(PerfEvent child, std::variant<Cpu, Thread> location,
                              int cgroup_fd = config().cgroup_fd);
    PerfEventGuard open_child(PerfEvent child, ExecutionScope location,
                              int cgroup_fd = config().cgroup_fd);

    void enable();
    void disable();

    void set_id(const uint64_t& id)
    {
        ioctl(fd_, PERF_EVENT_IOC_ID, &id);
    }

    void set_output(const PerfEventGuard& other_ev);
    void set_syscall_filter();

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
        T val;

        if (::read(fd_, &val, sizeof(val)) == -1)
        {
            throw_errno();
        }

        return val;
    }

    ~PerfEventGuard()
    {
        close(fd_);
    }

protected:
    int fd_;
    PerfEvent ev_;
};

} // namespace perf
} // namespace lo2s
