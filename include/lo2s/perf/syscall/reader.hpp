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
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>

#include <algorithm>

#include <cstdint>
extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::syscall
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

    Reader(Reader&) = delete;
    Reader& operator=(Reader&) = delete;

    ~Reader() = default;

    void stop()
    {
        enter_ev_.disable();
        // This should not be necessary because exit is attached to enter, but it can not hurt.
        exit_ev_.disable();

        this->read();
    }

protected:
    static EventGuard create_enter_ev(ExecutionScope scope)
    {
        tracepoint::TracepointEventAttr enter_event =
            EventComposer::instance().create_tracepoint_event("raw_syscalls:sys_enter");

        enter_event.set_sample_type(PERF_SAMPLE_IDENTIFIER);

        return enter_event.open(scope, config().cgroup_fd);
    }

    static EventGuard create_exit_ev(ExecutionScope scope)
    {
        tracepoint::TracepointEventAttr exit_event =
            EventComposer::instance().create_tracepoint_event("raw_syscalls:sys_exit");
        exit_event.set_sample_type(PERF_SAMPLE_IDENTIFIER);
        return exit_event.open(scope, config().cgroup_fd);
    }

    using EventReader<T>::init_mmap;
    uint64_t sys_enter_id = 0;
    uint64_t sys_exit_id = 0;

private:
    Reader(ExecutionScope scope)
    : scope_(scope), enter_ev_(create_enter_ev(scope)), exit_ev_(create_exit_ev(scope))
    {

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

    Reader& operator=(Reader&& other) noexcept
    {
        std::swap(scope_, other.scope_);
        std::swap(enter_ev_, other.enter_ev_);
        std::swap(exit_ev_, other.exit_ev_);
        std::swap(sys_enter_id, other.sys_enter_id);
        std::swap(sys_exit_id, other.sys_exit_id);
    }

    Reader(Reader&& other) noexcept
    : EventReader<T>(std::forward<perf::EventReader<T>>(other)), scope_(other.scope_)
    {
        std::swap(enter_ev_, other.enter_ev_);
        std::swap(exit_ev_, other.exit_ev_);
        std::swap(sys_enter_id, other.sys_enter_id);
        std::swap(sys_exit_id, other.sys_exit_id);
    }

    friend T;

    ExecutionScope scope_;
    EventGuard enter_ev_;
    EventGuard exit_ev_;
};

} // namespace lo2s::perf::syscall
