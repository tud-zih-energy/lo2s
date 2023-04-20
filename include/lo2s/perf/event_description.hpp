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

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/topology.hpp>

#include <set>
#include <string>

extern "C"
{
#include <linux/perf_event.h>
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

struct EventDescription
{
    EventDescription(const std::string& name, perf_type_id type, std::uint64_t config,
                     std::uint64_t config1 = 0, std::set<Cpu> cpus = std::set<Cpu>(),
                     bool is_uncore = false, std::set<Cpu> cpumask = std::set<Cpu>(),
                     double scale = 1, std::string unit = "#")
    : name_(name), type_(type), config_(config), config1_(config1), cpus_(cpus),
      is_uncore_(is_uncore), cpumask_(cpumask), scale_(scale), unit_(unit)
    {
        struct perf_event_attr attr = perf_event_attr();

        int proc_fd = perf_event_open(&attr, ExecutionScope(Thread(0)), -1, 0);
        int sys_fd = perf_event_open(&attr, ExecutionScope(*supported_cpus().begin()), -1, 0);

        if (sys_fd == -1 && proc_fd == -1)
        {
            attr.exclude_kernel = 1;
            proc_fd = perf_event_open(&attr, ExecutionScope(Thread(0)), -1, 0);
            sys_fd = perf_event_open(&attr, ExecutionScope(*supported_cpus().begin()), -1, 0);
        }

        if (sys_fd == -1 && proc_fd == -1)
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

            availability_ = Availability::UNAVAILABLE;
        }
        else if (sys_fd == -1)
        {
            availability_ = Availability::PROCESS_MODE;
        }
        else if (proc_fd == -1)
        {
            availability_ = Availability::SYSTEM_MODE;
        }
        else
        {
            availability_ = Availability::UNIVERSAL;
        }
    }

    EventDescription()
    : name_(""), type_(static_cast<perf_type_id>(-1)), config_(0), config1_(0), scale_(1),
      unit_("#"), availability_(Availability::UNAVAILABLE)
    {
    }

    const std::set<Cpu>& supported_cpus() const
    {
        if (cpus_.empty())
        {
            return Topology::instance().cpus();
        }
        return cpus_;
    }

    bool is_supported_in(ExecutionScope scope) const
    {
        // per-process should always work. the counter will just not count if the process is
        // scheduled on a core that is not supprted by that counter
        if (is_uncore_)
        {
            return scope.is_cpu() && cpumask_.count(scope.as_cpu());
        }
        return scope.is_thread() || cpus_.empty() || cpus_.count(scope.as_cpu());
    }

    friend bool operator==(const EventDescription& lhs, const EventDescription& rhs)
    {
        return (lhs.type_ == rhs.type_) && (lhs.config_ == rhs.config_) &&
               (lhs.config1_ == rhs.config1_);
    }

    friend bool operator<(const EventDescription& lhs, const EventDescription& rhs)
    {
        if (lhs.type_ == rhs.type_)
        {
            if (lhs.config_ == rhs.config_)
            {
                return lhs.config1_ < rhs.config1_;
            }
            return lhs.config_ < rhs.config_;
        }
        return lhs.type_ < rhs.type_;
    }

    struct perf_event_attr perf_event_attr() const
    {
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(struct perf_event_attr));

        attr.size = sizeof(struct perf_event_attr);

        attr.type = type_;
        attr.config = config_;
        attr.config1 = config1_;

        return attr;
    }

    std::string name() const
    {
        return name_;
    }

    std::string description() const
    {
        if (is_uncore_)
        {
            return fmt::format("{} [UNC]", name_);
        }

        if (availability_ == Availability::UNIVERSAL)
        {
            return name_;
        }
        else if (availability_ == Availability::SYSTEM_MODE)
        {
            return fmt::format("{} [SYS]", name_);
        }
        else if (availability_ == Availability::PROCESS_MODE)
        {
            return fmt::format("{} [PROC]", name_);
        }

        return "";
    }

    bool is_valid() const
    {
        return availability_ != Availability::UNAVAILABLE;
    }

    double scale() const
    {
        return scale_;
    }

    std::string unit() const
    {
        return unit_;
    }

    int open_counter(ExecutionScope scope, int group_fd)
    {
        struct perf_event_attr perf_attr = perf_event_attr();
        perf_attr.sample_period = 0;
        perf_attr.exclude_kernel = config().exclude_kernel;
        // Needed when scaling multiplexed events, and recognize activation phases
        perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

#if !defined(USE_HW_BREAKPOINT_COMPAT) && defined(USE_PERF_CLOCKID)
        perf_attr.use_clockid = config().use_clockid;
        perf_attr.clockid = config().clockid;
#endif

        int fd = perf_try_event_open(&perf_attr, scope, group_fd, 0, config().cgroup_fd);
        if (fd < 0)
        {
            Log::error() << "perf_event_open for counter failed";
            throw_errno();
        }
        return fd;
    }

private:
    std::string name_;
    perf_type_id type_;
    std::uint64_t config_;
    std::uint64_t config1_;
    std::set<Cpu> cpus_;
    bool is_uncore_;
    std::set<Cpu> cpumask_;
    double scale_;
    std::string unit_;
    Availability availability_;
};
} // namespace perf
} // namespace lo2s
