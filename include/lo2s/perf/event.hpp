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

#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>

#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <type_traits>
#include <variant>

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

enum class EventFlag
{
    SAMPLE_ID_ALL,
    COMM,
    CONTEXT_SWITCH,
    MMAP,
    EXCLUDE_KERNEL,
    TASK,
    ENABLE_ON_EXEC,
    DISABLED
};

class EventGuard;

/**
 * Base class for all Event types
 * contains common attributes
 */

class Event
{
public:
    Event(const std::string name, perf_type_id type, std::uint64_t config,
          std::uint64_t config1 = 0);

    /**
     * returns an opened instance of any Event object
     */
    EventGuard open(std::variant<Cpu, Thread> location, int cgroup_fd = -1);
    EventGuard open(ExecutionScope location, int cgroup_fd = -1);

    /**
     * returns an opened instance of a Event object after formating it as a leader Event
     */
    EventGuard open_as_group_leader(ExecutionScope location, int cgroup_fd = -1);

    const Availability& availability() const
    {
        return availability_;
    };

    std::string name() const
    {
        return name_;
    }

    std::set<Cpu> cpus() const
    {
        return cpus_;
    }

    std::string unit() const
    {
        return unit_;
    }

    perf_event_attr& attr()
    {
        return attr_;
    }

    double scale() const
    {
        return scale_;
    }

    void scale(double scale)
    {
        scale_ = scale;
    }

    void unit(const std::string& unit)
    {
        unit_ = unit;
    }

    void set_clockid(int64_t clockid)
    {
        if (clockid == -1)
        {
            attr_.use_clockid = 0;
        }
        else
        {
            attr_.use_clockid = 1;
            attr_.clockid = clockid;
        }
    }

    void set_read_format(uint64_t read_format)
    {
        attr_.read_format = read_format;
    }

    void set_flags(const std::vector<EventFlag>& flags)
    {
        for (const auto& flag : flags)
        {
            switch (flag)
            {
            case EventFlag::SAMPLE_ID_ALL:
                attr_.sample_id_all = 1;
                break;
            case EventFlag::COMM:
                attr_.comm = 1;
                break;
            case EventFlag::CONTEXT_SWITCH:
                attr_.context_switch = 1;
                break;
            case EventFlag::MMAP:
                attr_.mmap = 1;
                break;
            case EventFlag::EXCLUDE_KERNEL:
                attr_.exclude_kernel = 1;
                break;
            case EventFlag::TASK:
                attr_.task = 1;
                break;
            case EventFlag::ENABLE_ON_EXEC:
                attr_.enable_on_exec = 1;
                break;
            case EventFlag::DISABLED:
                attr_.disabled = 1;
                break;
            }
        }
    }

    bool get_flag(const EventFlag flag)
    {
        switch (flag)
        {
        case EventFlag::SAMPLE_ID_ALL:
            return attr_.sample_id_all == 1;
        case EventFlag::COMM:
            return attr_.comm == 1;
        case EventFlag::CONTEXT_SWITCH:
            return attr_.context_switch == 1;
        case EventFlag::MMAP:
            return attr_.mmap == 1;
        case EventFlag::EXCLUDE_KERNEL:
            return attr_.exclude_kernel == 1;
        case EventFlag::TASK:
            return attr_.task == 1;
        case EventFlag::ENABLE_ON_EXEC:
            return attr_.enable_on_exec == 1;
        case EventFlag::DISABLED:
            return attr_.disabled == 1;
        default:
            return false;
        }
    }

    uint64_t get_precise_ip()
    {
        return attr_.precise_ip;
    }

    void set_precise_ip(uint64_t precise_ip)
    {
        if (precise_ip > 3)
        {
            throw std::runtime_error("precise_ip set to > 3!");
        }
        attr_.precise_ip = precise_ip;
    }

    void set_sample_type(uint64_t format)
    {
        attr_.sample_type |= format;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Event& event);

    void set_watermark(uint64_t bytes)
    {
        attr_.watermark = 1;
        attr_.wakeup_watermark = static_cast<uint32_t>(bytes);
    }

    void sample_period(const int& period);
    void sample_freq(const uint64_t& freq);
    void event_attr_update(std::uint64_t value, const std::string& format);

    const std::set<Cpu>& supported_cpus() const;

    bool is_valid() const;
    bool event_is_openable();

    bool is_available_in(ExecutionScope scope) const
    {
        if (availability_ == Availability::UNAVAILABLE)
        {
            return false;
        }
        if (scope.is_thread() || scope.is_process())
        {
            return availability_ != Availability::SYSTEM_MODE;
        }
        else
        {
            return availability_ != Availability::PROCESS_MODE &&
                   (cpus_.empty() || cpus_.count(scope.as_cpu()));
        }
    }

    bool degrade_precision();

    friend bool operator==(const Event& lhs, const Event& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr)) == 0;
    }

    friend bool operator<(const Event& lhs, const Event& rhs)
    {
        return memcmp(&rhs.attr_, &lhs.attr_, sizeof(struct perf_event_attr)) < 0;
    }

    friend bool operator>(const Event& lhs, const Event& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr)) > 0;
    }

protected:
    void update_availability();
    struct perf_event_attr attr_;
    double scale_ = 1;
    std::string unit_ = "#";

    std::string name_;
    std::set<Cpu> cpus_;
    Availability availability_ = Availability::UNAVAILABLE;
};

class SimpleEvent : public Event
{
public:
    SimpleEvent(const std::string name, perf_type_id type, std::uint64_t config,
                std::uint64_t config1 = 0);
    static SimpleEvent raw(std::string name);
};

class BreakpointEvent : public Event
{
public:
    BreakpointEvent(uint64_t addr, uint64_t bp_type)
    : Event(std::to_string(addr), PERF_TYPE_BREAKPOINT, 0)
    {

        attr_.bp_type = bp_type;
        attr_.bp_addr = addr;
        attr_.bp_len = HW_BREAKPOINT_LEN_8;
    }
};

/**
 * Contains an event parsed from sysfs
 * @note call on use_sampling_options() after creation to get a valid
 * event, otherwise the availability will be set to UNAVAILABLE
 */
class SysfsEvent : public Event
{
public:
    SysfsEvent(const std::string ev_name);

    void make_invalid();

private:
    void parse_pmu_path(const std::string& ev_name);
    std::filesystem::path pmu_path_;
    std::string pmu_name_;
};

/**
 * Contains an opened instance of Event.
 * Use any Event.open() method to construct an object
 */
class EventGuard
{
public:
    EventGuard(Event& ev, std::variant<Cpu, Thread> location, int group_fd, int cgroup_fd);

    EventGuard() = delete;
    EventGuard(const EventGuard& other) = delete;
    EventGuard& operator=(const EventGuard&) = delete;

    EventGuard(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
    }

    EventGuard& operator=(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        return *this;
    }

    /**
     * opens child as a counter of the calling (leader) event
     */
    EventGuard open_child(Event child, ExecutionScope location, int cgroup_fd = -1);

    void enable();
    void disable();

    uint64_t get_id()
    {
        uint64_t id;
        if (ioctl(fd_, PERF_EVENT_IOC_ID, &id) == -1)
        {
            throw_errno();
        }

        return id;
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
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

protected:
    int fd_ = -1;
};

} // namespace perf
} // namespace lo2s
