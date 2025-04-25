/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018, Technische Universitaet Dresden, Germany
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
#include <lo2s/ringbuf_events.hpp>

#include <fmt/core.h>
#include <string.h>

namespace lo2s
{
namespace omp
{
enum class OMPType : uint64_t
{
    PARALLEL = 0,
    MASTER = 1,
    SYNC = 2,
    LOOP = 3,
    WORKSHARE = 4,
    OTHER = 5
};

enum class EventType : uint64_t
{
    OMPT_ENTER = 0,
    OMPT_EXIT = 1
};

struct OMPTCctx
{
    OMPType type;
    uint64_t tid;
    uint64_t addr;
    uint64_t num_threads;

    friend bool operator<(const OMPTCctx& lhs, const OMPTCctx& rhs)
    {
        if (lhs.type == rhs.type)
        {
            if (lhs.num_threads == rhs.num_threads)
            {
                return lhs.addr < rhs.addr;
            }
            else
            {
                return lhs.num_threads < rhs.num_threads;
            }
        }
        return lhs.type < rhs.type;
    }

    friend bool operator==(const OMPTCctx& lhs, const OMPTCctx& rhs)
    {
        return lhs.type == rhs.type && lhs.num_threads == rhs.num_threads && lhs.addr == rhs.addr;
    }

    friend bool operator!=(const OMPTCctx& lhs, const OMPTCctx& rhs)
    {
        return lhs.type != rhs.type || lhs.num_threads != rhs.num_threads || lhs.addr != rhs.addr;
    }

    std::string name() const
    {
        switch (type)
        {
        case lo2s::omp::OMPType::MASTER:
            return fmt::format("master {}", addr);
        case lo2s::omp::OMPType::PARALLEL:
            return fmt::format("parallel {} {}", num_threads, addr);
        case lo2s::omp::OMPType::SYNC:
            return fmt::format("sync {}", addr);
        case OMPType::WORKSHARE:
            return fmt::format("workshare {}", addr);
        case OMPType::LOOP:
            return fmt::format("loop {}", addr);
        case OMPType::OTHER:
            return fmt::format("other {}", addr);
        }
        return "";
    }
};

struct ompt_enter
{
    struct event_header header;
    uint64_t tp;
    OMPTCctx cctx;
};

struct ompt_exit
{
    struct event_header header;
    uint64_t tp;
    OMPTCctx cctx;
};

} // namespace omp
} // namespace lo2s
