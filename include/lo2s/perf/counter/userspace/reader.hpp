// SPDX-FileCopyrightText: 2021 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/perf/event_attr.hpp>

#include <vector>

namespace lo2s::perf::counter::userspace
{

template <class T>
class Reader
{
public:
    void read();

    int fd()
    {
        return timer_fd_;
    }

protected:
    CounterCollection counter_collection_;
    UserspaceCounterBuffer counter_buffer_;
    int timer_fd_;

    std::vector<EventGuard> counters_;
    std::vector<UserspaceReadFormat> data_;

private:
    Reader(ExecutionScope scope);
    friend T;
};
} // namespace lo2s::perf::counter::userspace
