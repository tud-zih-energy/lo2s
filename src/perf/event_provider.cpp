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

#include <lo2s/log.hpp>
#include <lo2s/config.hpp>
#include <lo2s/perf/event_provider.hpp>

#include <sstream>

extern "C" {
#include <linux/perf_event.h>
#include <linux/version.h>
#include <syscall.h>
#include <unistd.h>
}

namespace
{
#define PERF_EVENT(name, type, id)                                                                 \
    {                                                                                              \
        (name), (type), (id)                                                                       \
    }
#define PERF_EVENT_HW(name, id) PERF_EVENT(name, PERF_TYPE_HARDWARE, PERF_COUNT_HW_##id)
#define PERF_EVENT_SW(name, id) PERF_EVENT(name, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_##id)

static const lo2s::platform::CounterDescription HW_EVENT_TABLE[] = {
    PERF_EVENT_HW("cpu-cycles", CPU_CYCLES),
    PERF_EVENT_HW("instructions", INSTRUCTIONS),
    PERF_EVENT_HW("cache-references", CACHE_REFERENCES),
    PERF_EVENT_HW("cache-misses", CACHE_MISSES),
    PERF_EVENT_HW("branch-instructions", BRANCH_INSTRUCTIONS),
    PERF_EVENT_HW("branch-misses", BRANCH_MISSES),
    PERF_EVENT_HW("bus-cycles", BUS_CYCLES),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
    PERF_EVENT_HW("stalled-cycles-frontend", STALLED_CYCLES_FRONTEND),
    PERF_EVENT_HW("stalled-cycles-backend", STALLED_CYCLES_BACKEND),
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
    PERF_EVENT_HW("ref-cycles", REF_CPU_CYCLES),
#endif
};

static const lo2s::platform::CounterDescription SW_EVENT_TABLE[] = {
    PERF_EVENT_SW("cpu-clock", CPU_CLOCK),
    PERF_EVENT_SW("task-clock", TASK_CLOCK),
    PERF_EVENT_SW("page-faults", PAGE_FAULTS),
    PERF_EVENT_SW("context-switches", CONTEXT_SWITCHES),
    PERF_EVENT_SW("cpu-migrations", CPU_MIGRATIONS),
    PERF_EVENT_SW("page-faults-minor", PAGE_FAULTS_MIN),
    PERF_EVENT_SW("page-faults-major", PAGE_FAULTS_MAJ),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
    PERF_EVENT_SW("aligment-faults", ALIGNMENT_FAULTS),
    PERF_EVENT_SW("emulation-faults", EMULATION_FAULTS),
#endif
    /* PERF_EVENT_SW("dummy", DUMMY), */
};

#define PERF_MAKE_CACHE_ID(id) (id)
#define PERF_MAKE_CACHE_OP_ID(id) ((id) << 8)
#define PERF_MAKE_CACHE_OP_RES_ID(id) ((id) << 16)

template <typename T>
struct string_to_id
{
    const char* name;
    T id;
};

static constexpr string_to_id<perf_hw_cache_id> CACHE_NAME_TABLE[] = {
    { "L1-dcache", PERF_COUNT_HW_CACHE_L1D }, { "L1-icache", PERF_COUNT_HW_CACHE_L1I },
    { "LLC", PERF_COUNT_HW_CACHE_LL },        { "dTLB", PERF_COUNT_HW_CACHE_DTLB },
    { "iTLB", PERF_COUNT_HW_CACHE_ITLB },     { "branch", PERF_COUNT_HW_CACHE_BPU },
};

static constexpr string_to_id<perf_hw_cache_op_id> CACHE_OPERATION_TABLE[] = {
    { "load", PERF_COUNT_HW_CACHE_OP_READ },
    { "store", PERF_COUNT_HW_CACHE_OP_WRITE },
    { "prefetch", PERF_COUNT_HW_CACHE_OP_PREFETCH },
};

static constexpr string_to_id<perf_hw_cache_op_result_id> CACHE_OP_RESULT_TABLE[] = {
    /* use plural suffix on access; perf userland tools use
       "(load|store|prefetche)s" to mean (load|store|prefetch)-accesses
       TODO: fix pluralization prefetch -> prefetch(e)s */
    { "s", PERF_COUNT_HW_CACHE_RESULT_ACCESS },
    { "-misses", PERF_COUNT_HW_CACHE_RESULT_MISS },
};

inline constexpr std::uint64_t make_cache_config(perf_hw_cache_id cache, perf_hw_cache_op_id op,
                                                 perf_hw_cache_op_result_id op_result)
{
    return cache | (op << 8) | (op_result << 16);
}

template <std::size_t N, typename T>
inline constexpr std::size_t array_size(T (&)[N])
{
    return N;
}
}

namespace lo2s
{
namespace perf
{

static bool supported_by_kernel(const platform::CounterDescription& ev)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = ev.type;
    attr.config = ev.config;
    attr.config1 = ev.config1;
    attr.exclude_kernel = lo2s::config().exclude_kernel;

    int fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
    if (fd == -1)
    {
        switch (errno)
        {
        case ENOTSUP:
            Log::warn() << "perf event not supported by the running kernel: " << ev.name;
            break;
        default:
            Log::warn() << "perf event not available: " << ev.name;
            break;
        }
        return false;
    }

    Log::debug() << "perf event available: " << ev.name;
    close(fd);
    return true;
}

static void populate_event_map(EventProvider::EventMap& map)
{
    Log::info() << "checking available events...";
    map.reserve(array_size(HW_EVENT_TABLE) + array_size(SW_EVENT_TABLE) +
                array_size(CACHE_NAME_TABLE) * array_size(CACHE_OPERATION_TABLE) *
                    array_size(CACHE_OP_RESULT_TABLE));
    for (const auto& ev : HW_EVENT_TABLE)
    {
        if (supported_by_kernel(ev))
        {
            map.emplace(ev.name, ev);
        }
    }

    for (const auto& ev : SW_EVENT_TABLE)
    {
        if (supported_by_kernel(ev))
        {
            map.emplace(ev.name, ev);
        }
    }

    std::stringstream name_fmt;
    for (const auto& cache : CACHE_NAME_TABLE)
    {
        for (const auto& operation : CACHE_OPERATION_TABLE)
        {
            for (const auto& op_result : CACHE_OP_RESULT_TABLE)
            {
                name_fmt.str(std::string());
                name_fmt << cache.name << '-' << operation.name << op_result.name;

                platform::CounterDescription ev(
                    name_fmt.str(), PERF_TYPE_HW_CACHE,
                    make_cache_config(cache.id, operation.id, op_result.id));

                if (supported_by_kernel(ev))
                {
                    map.emplace(name_fmt.str(), ev);
                }
            }
        }
    }
}

EventProvider::EventProvider()
{
    populate_event_map(event_map_);
}
}
}