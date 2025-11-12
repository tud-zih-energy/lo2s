/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2021,
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

#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/helpers/timer_fd.hpp>

#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace userspace
{

template <class T>
class Reader
{
public:
    Reader(ExecutionScope scope);

    void read();

    WeakFd get_weak_fd()
    {
        return timer_fd_.to_weak();
    }

protected:
    CounterCollection counter_collection_;
    UserspaceCounterBuffer counter_buffer_;
    TimerFd timer_fd_;

    std::vector<EventGuard> counters_;
    std::vector<UserspaceReadFormat> data_;
};
} // namespace userspace
} // namespace counter
} // namespace perf
} // namespace lo2s
