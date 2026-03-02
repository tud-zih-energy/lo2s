// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/types/cpu.hpp>

#include <string>
#include <utility>

#include <cassert>
#include <cstddef>
#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::tracepoint
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
            const auto* input_cstr = reinterpret_cast<const char*>(raw_data_ + field.offset());
            size_t i = 0;
            for (i = 0; i < field.size() && input_cstr[i] != '\0'; i++)
            {
                ret[i] = input_cstr[i];
            }
            ret.resize(i);
            return ret;
        }

        template <typename TT>
        TT _get(ptrdiff_t offset) const
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

    Reader(Reader&) = delete;
    Reader& operator=(Reader&) = delete;

    ~Reader() = default;

    void stop()
    {
        ev_instance_.disable();
        this->read();
    }

private:
    Reader(Reader&& other) noexcept : EventReader<T>(std::forward<perf::EventReader<T>>(other))
    {
        std::swap(ev_instance_, other.ev_instance_);
    }

    Reader& operator=(Reader&& other) noexcept
    {
        std::swap(ev_instance_, other.ev_instance_);
    }

    Reader(Cpu cpu, tracepoint::TracepointEventAttr ev)
    : ev_instance_(create_tracepoint_ev(cpu, ev)), event_(std::move(ev))
    {
        try
        {
            init_mmap(ev_instance_.get_fd());
            Log::debug() << "perf_tracepoint_reader mmap initialized";

            ev_instance_.enable();
        }
        catch (...)
        {
            throw;
        }
    }

    static EventGuard create_tracepoint_ev(Cpu cpu, tracepoint::TracepointEventAttr& ev)
    {
        auto eg = ev.open(cpu.as_scope(), config().perf.cgroup_fd);
        Log::debug() << "Opened perf_sample_tracepoint_reader for " << cpu << " with id "
                     << ev.id();
        return eg;
    }

    friend T;
    EventGuard ev_instance_;

protected:
    using EventReader<T>::init_mmap;
    TracepointEventAttr event_;
};

} // namespace lo2s::perf::tracepoint
