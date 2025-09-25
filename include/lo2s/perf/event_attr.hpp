/* This file is part of the lo2s software.
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

class EventGuard;

/**
 * Base class for all Event types
 * contains common attributes
 */

class EventAttr
{
public:
    EventAttr(const std::string& name, perf_type_id type, std::uint64_t config,
              std::uint64_t config1 = 0);

    class InvalidEvent : public std::runtime_error
    {
    public:
        InvalidEvent(const std::string& event_description)
        : std::runtime_error(std::string{ "Invalid event: " } + event_description)
        {
        }
    };

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

    void set_clockid(std::optional<clockid_t> clockid)
    {
        if (!clockid.has_value())
        {
            attr_.use_clockid = 0;
        }
        else
        {
            attr_.use_clockid = 1;
            attr_.clockid = clockid.value();
        }
    }

    void set_read_format(uint64_t read_format)
    {
        attr_.read_format = read_format;
    }

    // Disables the event, so it can be later manually enabled with
    // ioctl(PERF_EVENT_IOC_ENABLE). Usually events start recording right at perf_event_open()
    void set_disabled()
    {
        attr_.disabled = 1;
    }

    bool disabled()
    {
        return attr_.disabled;
    }

    // Enables generation of comm events. comm events are generated on a process
    // or thread name change
    void set_comm()
    {
        attr_.comm = 1;
    }

    bool comm()
    {
        return attr_.comm;
    }

    // Start event recording on exec in the recorded process.
    void set_enable_on_exec()
    {
        attr_.enable_on_exec = 1;
    }

    bool enable_on_exec()
    {
        return attr_.enable_on_exec;
    }

    // Enables generation of task events. task events are generated on fork/exit
    void set_task()
    {
        attr_.task = 1;
    }

    bool task()
    {
        return attr_.task;
    }

    // Exclude the activity of the kernel from this event. This includes:
    // 1. samples from inside the kernel
    // 2. parts of the callstack that are inside the kernel
    // 2. metrics do not increase inside the kernel
    //
    // Setting this is required at perf_event_paranoid=2
    void set_exclude_kernel()
    {
        attr_.exclude_kernel = 1;
    }

    bool exclude_kernel()
    {
        return attr_.exclude_kernel;
    }

    // Enables generation of mmap events. mmap events are generated when:
    // 1. A library/executable is loaded into a process, including on exec
    // 2. mmap is called with PROT_EXEC
    void set_mmap()
    {
        attr_.mmap = 1;
    }

    bool mmap()
    {
        return attr_.mmap;
    }

    // Enables generation of context switch events. Context switch events
    // are generated when the kernels switches the process running on a CPU.
    void set_context_switch()
    {
        attr_.context_switch = 1;
    }

    bool context_switch()
    {
        return attr_.context_switch;
    }

    // Sample events can be configured to additionally include information such as a timestmap, PID,
    // TID, etc. sample_id_all includes this information not only in sample events, but also in all
    // oter events, such as mmap and  context switch events;
    void set_sample_id_all()
    {
        attr_.sample_id_all = 1;
    }

    bool sample_id_all()
    {
        return attr_.sample_id_all;
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

    friend std::ostream& operator<<(std::ostream& stream, const EventAttr& event);

    void set_watermark(uint64_t bytes)
    {
        attr_.watermark = 1;
        attr_.wakeup_watermark = static_cast<uint32_t>(bytes);
    }

    void sample_period(int period);
    void sample_freq(uint64_t freq);
    void event_attr_update(std::uint64_t value, const std::string& format);

    const std::set<Cpu>& supported_cpus() const;

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

    friend bool operator==(const EventAttr& lhs, const EventAttr& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr)) == 0;
    }

    friend bool operator<(const EventAttr& lhs, const EventAttr& rhs)
    {
        return memcmp(&rhs.attr_, &lhs.attr_, sizeof(struct perf_event_attr)) < 0;
    }

    friend bool operator>(const EventAttr& lhs, const EventAttr& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr)) > 0;
    }

protected:
    static std::set<Cpu> get_cpu_set_for(EventAttr attr);
    void update_availability();

    struct perf_event_attr attr_;

    double scale_ = 1;
    std::string unit_ = "#";
    std::string name_;
    std::set<Cpu> cpus_;
    Availability availability_ = Availability::UNAVAILABLE;
};

class PredefinedEventAttr : public EventAttr
{
public:
    PredefinedEventAttr(const std::string& name, perf_type_id type, std::uint64_t config,
                        std::uint64_t config1 = 0);
};

class RawEventAttr : public EventAttr
{
public:
    RawEventAttr(const std::string& name);
};

#ifndef USE_HW_BREAKPOINT_COMPAT
class BreakpointEventAttr : public EventAttr
{
public:
    BreakpointEventAttr(uint64_t addr, uint64_t bp_type)
    : EventAttr(std::to_string(addr), PERF_TYPE_BREAKPOINT, 0)
    {
        attr_.bp_type = bp_type;
        attr_.bp_addr = addr;
        attr_.bp_len = HW_BREAKPOINT_LEN_8;
    }
};
#endif

/**
 * Contains an event parsed from sysfs
 * @note call on use_sampling_options() after creation to get a valid
 * event, otherwise the availability will be set to UNAVAILABLE
 */
class SysfsEventAttr : public EventAttr

{
public:
    SysfsEventAttr(const std::string& ev_name);
};

/**
 * Contains an opened instance of Event.
 * Use any Event.open() method to construct an object
 */
class EventGuard
{
public:
    EventGuard(EventAttr& ev, std::variant<Cpu, Thread> location, int group_fd, int cgroup_fd);

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
    EventGuard open_child(EventAttr& child, ExecutionScope location, int cgroup_fd = -1);

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
