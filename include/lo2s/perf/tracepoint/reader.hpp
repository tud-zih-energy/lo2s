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
#include <lo2s/log.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/util.hpp>

#include <filesystem>
#include <ios>
#include <optional>
#include <stdexcept>

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

    Reader(Cpu cpu, perf::tracepoint::TracepointEventAttr ev) : event_(ev), cpu_(cpu)
    {
        ev_instance_ = event_.open(cpu_.as_scope(), config().cgroup_fd);

        Log::debug() << "Opened perf_sample_tracepoint_reader for " << cpu_ << " with id "
                     << event_.id();

        try
        {
            init_mmap(ev_instance_.value().get_fd());
            Log::debug() << "perf_tracepoint_reader mmap initialized";

            ev_instance_.value().enable();
        }
        catch (...)
        {
            throw;
        }
    }

    Reader(Reader&& other)
    : EventReader<T>(std::forward<perf::EventReader<T>>(other)), cpu_(other.cpu_)
    {
        std::swap(ev_instance_, other.ev_instance_);
    }

    void stop()
    {
        ev_instance_.value().disable();
        this->read();
    }

protected:
    using EventReader<T>::init_mmap;
    TracepointEventAttr event_;

private:
    Cpu cpu_;
    std::optional<EventGuard> ev_instance_;
};

} // namespace tracepoint
} // namespace perf
} // namespace lo2s
