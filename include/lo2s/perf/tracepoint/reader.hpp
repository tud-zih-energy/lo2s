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

#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/util.hpp>

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
namespace tracepoint
{
template <class T>
class Reader : public EventReader<T>
{
public:
    struct RecordDynamicFormat
    {
        uint64_t get(const EventField& field) const
        {
            switch (field.size())
            {
            case 1:
                return _get<int8_t>(field.offset());
            case 2:
                return _get<int16_t>(field.offset());
            case 4:
                return _get<int32_t>(field.offset());
            case 8:
                return _get<int64_t>(field.offset());
            default:
                // We do check this before setting up the event
                Log::warn() << "Trying to get field " << field.name()
                            << " of invalid size: " << field.size();
                return 0;
            }
        }

        std::string get_str(const EventField& field) const
        {
            std::string ret;
            ret.resize(field.size());
            auto input_cstr = reinterpret_cast<const char*>(raw_data_ + field.offset());
            size_t i;
            for (i = 0; i < field.size() && input_cstr[i] != '\0'; i++)
            {
                ret[i] = input_cstr[i];
            }
            ret.resize(i);
            return ret;
        }

        template <typename TT>
        const TT _get(ptrdiff_t offset) const
        {
            assert(offset >= 0);
            assert(offset + sizeof(TT) <= size_);
            return *(reinterpret_cast<const TT*>(raw_data_ + offset));
        }

        // DO NOT TOUCH, MUST NOT BE size_t!!!!
        uint32_t size_;
        std::byte raw_data_[1]; // Can I still not [0] with ISO-C++ :-(
    };

    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
        // uint32_t size;
        // char data[size];
        RecordDynamicFormat raw_data;
    };

    Reader(Cpu cpu, int event_id) : cpu_(cpu)
    {
        struct perf_event_attr attr = common_perf_event_attrs();
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.config = event_id;
        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME;

        fd_ = perf_event_open(&attr, cpu.as_scope(), -1, 0, config().cgroup_fd);
        if (fd_ < 0)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }
        Log::debug() << "Opened perf_sample_tracepoint_reader for " << cpu_ << " with id "
                     << event_id;

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

            auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
            Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
            if (ret == -1)
            {
                throw_errno();
            }
        }
        catch (...)
        {
            close(fd_);
            throw;
        }
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

private:
    Cpu cpu_;
    int fd_ = -1;
    const static std::filesystem::path base_path;
};

template <typename T>
const std::filesystem::path Reader<T>::base_path =
    std::filesystem::path("/sys/kernel/debug/tracing/events");
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
