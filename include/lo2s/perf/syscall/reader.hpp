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

#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <filesystem>

#include <ios>

#include <cstddef>

extern "C"
{
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
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
        struct perf_event_attr attr = common_perf_event_attrs();
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.config = tracepoint::EventFormat("raw_syscalls:sys_enter").id();
        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME | PERF_SAMPLE_IDENTIFIER;

        fd_ = perf_event_open(&attr, cpu.as_scope(), -1, 0, config().cgroup_fd);
        if (fd_ < 0)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }
        
        attr.config = tracepoint::EventFormat("raw_syscalls:sys_exit").id();
        int other_fd  = perf_event_open(&attr, cpu.as_scope(), -1, 0, config().cgroup_fd);
        if (other_fd < 0)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
            close(fd_);
        }

        try
        {
            // asynchronous delivery
            // if (fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK))
            if (fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_);
            Log::debug() << "perf_tracepoint_reader mmap initialized";
        if (ioctl(other_fd, PERF_EVENT_IOC_SET_OUTPUT, fd_) == -1)
        {
            throw_errno();
        }
        
            auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
            Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
            if (ret == -1)
            {
                throw_errno();
            }
            
            ret = ioctl(other_fd, PERF_EVENT_IOC_ENABLE);
            Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
            if (ret == -1)
            {
                throw_errno();
            }
        }
        catch (...)
        {
            close(other_fd);
            close(fd_);
            throw;
        }

        ioctl(fd_, PERF_EVENT_IOC_ID, &sys_enter_id);
        Log::error() << sys_enter_id;
        ioctl(other_fd, PERF_EVENT_IOC_ID, &sys_exit_id);
    }

    Reader(Reader&& other)
    : EventReader<T>(std::forward<perf::EventReader<T>>(other)), cpu_(other.cpu_)
    {
        std::swap(fd_, other.fd_);
    }

    ~Reader()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    void stop()
    {
        auto ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE);
        Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_DISABLE) = " << ret;
        if (ret == -1)
        {
            throw_errno();
        }
        this->read();
    }

protected:
    using EventReader<T>::init_mmap;
    uint64_t sys_enter_id;
    uint64_t sys_exit_id;
private:
    Cpu cpu_;
    int fd_ = -1;

    const static std::filesystem::path base_path;
};

template <typename T>
const std::filesystem::path
    Reader<T>::base_path = std::filesystem::path("/sys/kernel/debug/tracing/events");
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
