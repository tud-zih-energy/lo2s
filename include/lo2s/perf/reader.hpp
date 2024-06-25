/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

class PerfEventInstance;

/**
 * Base class for all Event types
 * contains common attributes
 */
class PerfEvent
{
public:
    PerfEvent(bool enable_on_exec);
    PerfEvent();

    /**
     * returns an opened instance of any PerfEvent object
     * @param group_fd file descriptor of the leader event, omit if there is no leader
     */
    PerfEventInstance open(std::variant<Cpu, Thread> location, int group_fd = -1,
                           int cgroup_fd = config().cgroup_fd);

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

    std::string get_unit() const
    {
        return unit_;
    }

    void set_unit(std::string unit)
    {
        unit_ = unit;
    }

    friend bool operator==(const PerfEvent& lhs, const PerfEvent& rhs)
    {
        return !memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    friend bool operator<(const PerfEvent& lhs, const PerfEvent& rhs)
    {
        return memcmp(&lhs.attr_, &rhs.attr_, sizeof(struct perf_event_attr));
    }

    ~PerfEvent()
    {
    }

protected:
    struct perf_event_attr attr_;
    double scale_ = 1;
    std::string unit_ = "#";
};

/**
 * Contains an event parsed from sysfs
 * @note call on either as_counter(), as_sample() or as_group_leader() after creation to get a valid
 * event, otherwise the availability will be set to UNAVAILABLE
 */
class SysfsEvent : public PerfEvent
{
public:
    SysfsEvent();
    SysfsEvent(bool enable_on_exec, const std::string& ev_name);
    SysfsEvent(std::string name, perf_type_id type, std::uint64_t config, std::uint64_t config1 = 0,
               std::set<Cpu> cpus = std::set<Cpu>());

    void as_counter();
    void as_group_leader();
    void as_sample();

    const std::set<Cpu>& supported_cpus() const;

    // automatically determine availability
    void set_availability();

    const Availability& get_availability() const
    {
        return availability_;
    };

    bool degrade_percision();

    bool is_valid() const;

    bool is_available_in(ExecutionScope scope) const
    {
        // per-process should always work. the counter will just not count if the process is
        // scheduled on a core that is not supprted by that counter
        return scope.is_thread() || cpus_.empty() || cpus_.count(scope.as_cpu());
    }

    std::string get_name() const
    {
        return name_;
    }

    std::set<Cpu> get_cpus() const
    {
        return cpus_;
    }

    void set_common_attrs(std::string name, perf_type_id type, std::uint64_t config,
                          std::uint64_t config1 = 0, std::set<Cpu> cpus = std::set<Cpu>())
    {
        name_ = name;
        cpus_ = cpus;
        attr_.type = type;
        attr_.config = config;
        attr_.config1 = config1;
    }

private:
    std::string name_ = "";
    std::set<Cpu> cpus_;
    Availability availability_ = Availability::UNAVAILABLE;
};

class TimeEvent : public PerfEvent
{
public:
    TimeEvent(bool enable_on_exec, uint64_t addr);
};

/**
 * Contains an event that is addressable via ID
 * @note Call on as_syscall() after creation if needed
 */
class TracepointEvent : public PerfEvent
{
public:
    TracepointEvent(bool enable_on_exec, int event_id);

    void as_syscall()
    {
        attr_.sample_type |= PERF_SAMPLE_IDENTIFIER;
    }
};

/**
 * Contains a raw event for testing
 */
class RawEvent : public PerfEvent
{
public:
    RawEvent(bool enable_on_exec);
};

/**
 * Contains an opened instance of PerfEvent.
 * Use PerfEvent.open() to construct an object
 */
class PerfEventInstance
{
public:
    PerfEventInstance();
    PerfEventInstance(PerfEvent& ev, std::variant<Cpu, Thread> location, int group_fd,
                      int cgroup_fd);

    PerfEventInstance(PerfEventInstance&) = delete;
    PerfEventInstance& operator=(const PerfEventInstance&) = delete;

    PerfEventInstance(PerfEventInstance&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
    }

    PerfEventInstance& operator=(PerfEventInstance&& other)
    {
        std::swap(fd_, other.fd_);
        std::swap(ev_, other.ev_);
        return *this;
    }

    void enable();
    void disable();

    void set_output(const PerfEventInstance& other_ev);
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
        if (::read(fd_, &val, sizeof(val)) != sizeof(T))
        {
            throw std::system_error(errno, std::system_category());
        }

        return val;
    }

    ~PerfEventInstance()
    {
        close(fd_);
    }

protected:
    int fd_;
    PerfEvent ev_;
};

} // namespace perf
} // namespace lo2s
