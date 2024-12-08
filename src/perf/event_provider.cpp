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

#include <lo2s/build_config.hpp>
#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#ifdef HAVE_LIBPFM
#include <lo2s/perf/pfm.hpp>
#endif
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include <cstring>

extern "C"
{
#include <linux/perf_event.h>
#include <linux/version.h>
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

static lo2s::perf::Event HW_EVENT_TABLE[] = {
    PERF_EVENT_HW("cpu-cycles", CPU_CYCLES),
    PERF_EVENT_HW("instructions", INSTRUCTIONS),
    PERF_EVENT_HW("cache-references", CACHE_REFERENCES),
    PERF_EVENT_HW("cache-misses", CACHE_MISSES),
    PERF_EVENT_HW("branch-instructions", BRANCH_INSTRUCTIONS),
    PERF_EVENT_HW("branch-misses", BRANCH_MISSES),
    PERF_EVENT_HW("bus-cycles", BUS_CYCLES),
#ifdef HAVE_PERF_EVENT_STALLED_CYCLES_FRONTEND
    PERF_EVENT_HW("stalled-cycles-frontend", STALLED_CYCLES_FRONTEND),
#endif
#ifdef HAVE_PERF_EVENT_STALLED_CYCLES_BACKEND
    PERF_EVENT_HW("stalled-cycles-backend", STALLED_CYCLES_BACKEND),
#endif
#ifdef HAVE_PERF_EVENT_REF_CYCLES
    PERF_EVENT_HW("ref-cycles", REF_CPU_CYCLES),
#endif
};

static lo2s::perf::Event SW_EVENT_TABLE[] = {
    PERF_EVENT_SW("cpu-clock", CPU_CLOCK),
    PERF_EVENT_SW("task-clock", TASK_CLOCK),
    PERF_EVENT_SW("page-faults", PAGE_FAULTS),
    PERF_EVENT_SW("context-switches", CONTEXT_SWITCHES),
    PERF_EVENT_SW("cpu-migrations", CPU_MIGRATIONS),
    PERF_EVENT_SW("minor-faults", PAGE_FAULTS_MIN),
    PERF_EVENT_SW("major-faults", PAGE_FAULTS_MAJ),
#ifdef HAVE_PERF_EVENT_ALIGNMENT_FAULTS
    PERF_EVENT_SW("alignment-faults", ALIGNMENT_FAULTS),
#endif
#ifdef HAVE_PERF_EVENT_EMULATION_FAULTS
    PERF_EVENT_SW("emulation-faults", EMULATION_FAULTS),
#endif
#ifdef HAVE_PERF_EVENT_DUMMY
    PERF_EVENT_SW("dummy", DUMMY),
#endif
#ifdef HAVE_PERF_EVENT_BPF_OUTPUT
    PERF_EVENT_SW("bpf-output", BPF_OUTPUT),
#endif
#ifdef HAVE_PERF_EVENT_CGROUP_SWITCHES
    PERF_EVENT_SW("cgroup-switches", CGROUP_SWITCHES),
#endif
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

static string_to_id<perf_hw_cache_id> CACHE_NAME_TABLE[] = {
    { "L1-dcache", PERF_COUNT_HW_CACHE_L1D }, { "L1-icache", PERF_COUNT_HW_CACHE_L1I },
    { "LLC", PERF_COUNT_HW_CACHE_LL },        { "dTLB", PERF_COUNT_HW_CACHE_DTLB },
    { "iTLB", PERF_COUNT_HW_CACHE_ITLB },     { "branch", PERF_COUNT_HW_CACHE_BPU },
#ifdef HAVE_PERF_EVENT_CACHE_NODE
    { "node", PERF_COUNT_HW_CACHE_NODE },
#endif
};

struct cache_op_and_result
{
    perf_hw_cache_op_id op_id;
    perf_hw_cache_op_result_id result_id;
};

static string_to_id<cache_op_and_result> CACHE_OPERATION_TABLE[] = {
    { "loads", { PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "stores", { PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "prefetches", { PERF_COUNT_HW_CACHE_OP_PREFETCH, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "load-misses", { PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS } },
    { "store-misses", { PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_MISS } },
    { "prefetch-misses", { PERF_COUNT_HW_CACHE_OP_PREFETCH, PERF_COUNT_HW_CACHE_RESULT_MISS } },
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
} // namespace

namespace lo2s
{
namespace perf
{

static void populate_event_map(std::unordered_map<std::string, Event>& map)
{
    Log::info() << "checking available events...";
    map.reserve(array_size(HW_EVENT_TABLE) + array_size(SW_EVENT_TABLE) +
                array_size(CACHE_NAME_TABLE) * array_size(CACHE_OPERATION_TABLE));
    for (auto& ev : HW_EVENT_TABLE)
    {
        Event event(ev);
        map.emplace(event.name(), event);
    }

    for (auto& ev : SW_EVENT_TABLE)
    {
        Event event(ev);
        map.emplace(event.name(), event);
    }

    std::stringstream name_fmt;
    for (auto& cache : CACHE_NAME_TABLE)
    {
        for (auto& operation : CACHE_OPERATION_TABLE)
        {
            name_fmt.str(std::string());
            name_fmt << cache.name << '-' << operation.name;

            // don't use EventProvider::instance() here, will cause recursive init
            map.emplace(name_fmt.str(),
                        EventProvider::create_event(name_fmt.str(), PERF_TYPE_HW_CACHE,
                                                    make_cache_config(cache.id, operation.id.op_id,
                                                                      operation.id.result_id)));
        }
    }
}

std::vector<SysfsEvent> EventProvider::get_pmu_events()
{
    std::vector<SysfsEvent> events;

    const std::filesystem::path pmu_devices("/sys/bus/event_source/devices");

    for (const auto& pmu : std::filesystem::directory_iterator(pmu_devices))
    {
        const auto pmu_path = pmu.path();

        const std::filesystem::path event_dir(pmu_path / "events");

        // some PMUs don't have any events, in that case event_dir doesn't exist
        if (!std::filesystem::is_directory(event_dir))
        {
            continue;
        }

        for (const auto& event : std::filesystem::directory_iterator(event_dir))
        {
            std::stringstream event_name;

            const auto event_path = event.path();
            const auto extension = event_path.extension();

            // ignore scaling and unit information
            if (extension == ".scale" || extension == ".unit")
            {
                continue;
            }

            // use std::filesystem::path::string, otherwise the paths are formatted quoted
            event_name << pmu_path.filename().string() << '/' << event_path.filename().string()
                       << '/';
            try
            {
                SysfsEvent event = EventProvider::instance().create_sysfs_event(event_name.str());
                events.emplace_back(event);
            }
            catch (const EventProvider::InvalidEvent& e)
            {
                Log::debug() << "Can not open event " << event_name.str() << ":" << e.what();
            }
        }
    }

    return events;
}

Event EventProvider::fallback_metric_leader_event()
{
    Log::debug() << "checking for metric leader event...";
    for (auto candidate : {
             "ref-cycles",
             "cpu-cycles",
             "bus-cycles",
         })
    {
        try
        {
            const Event ev = get_event_by_name(candidate);
            Log::debug() << "found suitable metric leader event: " << candidate;
            return ev;
        }
        catch (const InvalidEvent& e)
        {
            Log::debug() << "not a suitable metric leader event: " << candidate;
        }
    }

    throw InvalidEvent{ "no suitable metric leader event found" };
}

/**
 * takes the name of an event, checks if it can be opened with each cpu and returns a PerfEvent
 * with a set of working cpus
 */
const Event raw_read_event(const std::string& ev_name)
{
    // Do not check whether the event_is_openable because we don't know whether we are in
    // system or process mode
    return EventProvider::instance().create_event(ev_name, PERF_TYPE_RAW,
                                                  std::stoull(ev_name.substr(1), nullptr, 16), 0);
}

EventProvider::EventProvider()
{
    populate_event_map(event_map_);
}

Event EventProvider::cache_event(const std::string& name)
{
    // Format for raw events is r followed by a hexadecimal number
    static const std::regex raw_regex("r[[:xdigit:]]{1,8}");

    // save event in event map; return a reference to the inserted event to
    // the caller.
    try
    {
        if (regex_match(name, raw_regex))
        {
            return event_map_.emplace(name, raw_read_event(name)).first->second;
        }
        else
        {
            SysfsEvent event = EventProvider::instance().create_sysfs_event(name, false);
            return event_map_.emplace(name, event).first->second;
        }
    }
    catch (const InvalidEvent& e)
    {
        // emplace unavailable Sampling Event
        event_map_.emplace(name, SysfsEvent());
        throw e;
    }
}

/**
 * takes the name of an event and looks it up in an internal event list.
 * @returns The corresponding PerfEvent if it is available
 * @throws InvalidEvent if the event is unavailable
 */
Event EventProvider::get_event_by_name(const std::string& name)
{
    auto& ev_map = instance().event_map_;
    auto event_it = ev_map.find(name);
    if (event_it != ev_map.end())
    {
        if (event_it->second.is_valid())
        {
            return event_it->second;
        }
        else
        {
            throw InvalidEvent("The event '" + name + "' is not available");
        }
    }
    else
    {
        return instance_mutable().cache_event(name);
    }
}

bool EventProvider::has_event(const std::string& name)
{
    auto& ev_map = instance().event_map_;
    const auto event_it = ev_map.find(name);
    if (event_it != ev_map.end())
    {
        return (event_it->second.is_valid());
    }
    else
    {
        try
        {
            instance_mutable().cache_event(name);
            return true;
        }
        catch (const InvalidEvent&)
        {
            return false;
        }
    }
}

std::vector<Event> EventProvider::get_predefined_events()
{
    const auto& ev_map = instance().event_map_;

    std::vector<Event> events;
    events.reserve(ev_map.size());

    for (const auto& event : ev_map)
    {
        if (event.second.is_valid())
        {
            events.push_back(std::move(event.second));
        }
    }

    return events;
}

// returns a standard TracepointEvent, can use config() if specified, otherwise sets default values
tracepoint::TracepointEvent EventProvider::create_tracepoint_event(const std::string& name,
                                                                   bool use_config,
                                                                   bool enable_on_exec)
{
    tracepoint::TracepointEvent event(name, enable_on_exec);
    event.sample_period(0);

    if (use_config)
    {
        apply_config_attrs(event);
    }
    else
    {
        apply_default_attrs(event);
    }

    return event;
}

// returns a Event with bp_addr set to local_time, uses config()
Event EventProvider::create_time_event(uint64_t local_time, bool enable_on_exec)
{
#ifndef USE_HW_BREAKPOINT_COMPAT
    Event event("Time", PERF_TYPE_BREAKPOINT, 0); // TODO: name for time events
#else
    Event event("Time", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
#endif

    event.sample_period(1); // may be overwritten by event.time_attrs
    event.time_attrs(local_time, enable_on_exec);

    apply_config_attrs(event);
    event.exclude_kernel(true); // overwrite config value

    return event;
}

// returns a standard Event, uses config() if possible, else sets default values
Event EventProvider::create_event(const std::string& name, perf_type_id type, std::uint64_t config,
                                  std::uint64_t config1)
{
    Event event(name, type, config, config1);
    event.sample_period(0);

    try
    {
        apply_config_attrs(event);
        event.exclude_kernel(true); // overwrite config value
    }
    catch (...)
    {
        apply_default_attrs(event);
    }

    return event;
}

// returns a SysfsEvent with sampling options enabled, uses config()
SysfsEvent EventProvider::create_sampling_event(bool enable_on_exec)
{
    SysfsEvent event(config().sampling_event, enable_on_exec);
    apply_config_attrs(event);

    event.sample_period(config().sampling_period);
    event.use_sampling_options(config().use_pebs, config().sampling, config().enable_cct);

    return event;
}

// returns a standard SysfsEvent, can use config() if specified, otherwise sets default values
SysfsEvent EventProvider::create_sysfs_event(const std::string& name, bool use_config)
{
    SysfsEvent event(name);
    event.sample_period(0);

    if (use_config)
    {
        apply_config_attrs(event);
    }
    else
    {
        apply_default_attrs(event);
    }

    return event;
}

void EventProvider::apply_config_attrs(Event& event)
{
    event.watermark(config().mmap_pages);
    event.exclude_kernel(config().exclude_kernel);
    event.clock_attrs(config().use_clockid, config().clockid);
}

void EventProvider::apply_default_attrs(Event& event)
{
    event.watermark(16);        // default mmap-pages value
    event.exclude_kernel(true); // enabled by default
    event.clock_attrs(true, CLOCK_MONOTONIC_RAW);
}

} // namespace perf
} // namespace lo2s
