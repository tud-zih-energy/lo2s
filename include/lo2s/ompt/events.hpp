// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <lo2s/rb/events.hpp>

#include <string>

#include <cstdint>

#include <fmt/format.h>

extern "C"
{
#include <unistd.h>
}

namespace lo2s::ompt
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
    OMPTCctx(OMPType type, const void* addr, uint64_t num_threads = 0)
    : type(type), tid(static_cast<int64_t>(::gettid())), addr(reinterpret_cast<uint64_t>(addr)),
      num_threads(num_threads)
    {
    }

    OMPType type;
    int64_t tid;
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
            return lhs.num_threads < rhs.num_threads;
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
        case OMPType::MASTER:
            return fmt::format("master {}", addr);
        case OMPType::PARALLEL:
            return fmt::format("parallel {} {}", num_threads, addr);
        case OMPType::SYNC:
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

} // namespace lo2s::ompt
