// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/perf/counter/counter_buffer.hpp>

#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace lo2s::perf::counter::userspace
{
struct UserspaceReadFormat
{
    uint64_t value;
    uint64_t time_enabled;
    uint64_t time_running;
};

class UserspaceCounterBuffer : public CounterBuffer<UserspaceCounterBuffer>
{
public:
    UserspaceCounterBuffer(std::size_t ncounters)
    : CounterBuffer<UserspaceCounterBuffer>(ncounters), current_(ncounters), previous_(ncounters)
    {
    }

    void read(std::vector<UserspaceReadFormat>& inbuf)
    {
        current_ = inbuf;
        update_buffers();
        std::swap(current_, previous_);
    }

    std::size_t diff_enabled(size_t nr)
    {
        return current_[nr].time_enabled - previous_[nr].time_enabled;
    }

    std::size_t diff_running(size_t nr)
    {
        return current_[nr].time_running - previous_[nr].time_running;
    }

    uint64_t diff_value(size_t nr)
    {
        return current_[nr].value - previous_[nr].value;
    }

protected:
    std::vector<UserspaceReadFormat> current_;
    std::vector<UserspaceReadFormat> previous_;
};

} // namespace lo2s::perf::counter::userspace
