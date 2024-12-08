/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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
#include <lo2s/log.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/util.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <ios>

#include <fmt/format.h>
extern "C"
{
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
}

namespace lo2s
{
namespace perf
{
namespace syscall
{
template <class T>
class Reader : public EventReader<T>
{
public:
    struct __attribute__((__packed__)) RecordSampleType
    {
        struct perf_event_header header;
        uint64_t id;
        uint64_t time;
        uint32_t size;
        uint16_t common_type;
        uint8_t common_flags;
        uint8_t common_preempt_count;
        uint32_t common_pid;
        int64_t syscall_nr;
        uint64_t args[6];
    };

    Reader(Cpu cpu) : cpu_(cpu)
    {
        tracepoint::TracepointEvent enter_event =
            EventProvider::instance().create_tracepoint_event("raw_syscalls:sys_enter");
        tracepoint::TracepointEvent exit_event =
            EventProvider::instance().create_tracepoint_event("raw_syscalls:sys_exit");

        try
        {
            enter_ev_ = enter_event.open(cpu_, config().cgroup_fd);
            exit_ev_ = exit_event.open(cpu_, config().cgroup_fd);
        }
        catch (const std::system_error& e)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }

        init_mmap(enter_ev_.get_fd());
        Log::debug() << "perf_tracepoint_reader mmap initialized";

        exit_ev_.set_output(enter_ev_);

        enter_ev_.set_syscall_filter(config().syscall_filter);
        exit_ev_.set_syscall_filter(config().syscall_filter);

        enter_ev_.enable();
        exit_ev_.enable();

        sys_enter_id = enter_ev_.get_id();
        sys_exit_id = exit_ev_.get_id();
    }

    Reader(Reader&& other)
    : EventReader<T>(std::forward<perf::EventReader<T>>(other)), cpu_(other.cpu_)
    {
        std::swap(enter_ev_, other.enter_ev_);
    }

    void stop()
    {
        enter_ev_.disable();
        this->read();
    }

protected:
    using EventReader<T>::init_mmap;
    uint64_t sys_enter_id;
    uint64_t sys_exit_id;

private:
    Cpu cpu_;
    EventGuard enter_ev_;
    EventGuard exit_ev_;
};

} // namespace syscall
} // namespace perf
} // namespace lo2s
