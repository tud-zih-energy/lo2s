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
#include <lo2s/perf/event_resolver.hpp>
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
#define PERF_EVENT(name, type, id) { (name), (type), (id) }
#define PERF_EVENT_SW(name, id) PERF_EVENT(name, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_##id)

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

std::vector<SysfsEventAttr> EventResolver::get_pmu_events()
{
    std::vector<SysfsEventAttr> events;

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
                SysfsEventAttr event(event_name.str());
                events.emplace_back(event);
            }
            catch (const EventAttr::InvalidEvent& e)
            {
                Log::debug() << "Can not open event " << event_name.str() << ":" << e.what();
            }
        }
    }

    return events;
}

std::vector<std::string> EventResolver::get_tracepoint_event_names()
{
    try
    {
        std::ifstream ifs_available_events;
        ifs_available_events.exceptions(std::ios::failbit | std::ios::badbit);

        ifs_available_events.open("/sys/kernel/debug/tracing/available_events");
        ifs_available_events.exceptions(std::ios::badbit);

        std::vector<std::string> available;

        for (std::string tracepoint; std::getline(ifs_available_events, tracepoint);)
        {
            available.emplace_back(std::move(tracepoint));
        }

        return available;
    }
    catch (const std::ios_base::failure& e)
    {
        Log::debug() << "Retrieving kernel tracepoint event names failed: " << e.what();
        return {};
    }
}

EventAttr EventResolver::fallback_metric_leader_event()
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
            const EventAttr ev = get_event_by_name(candidate);
            Log::debug() << "found suitable metric leader event: " << candidate;
            return ev;
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            Log::debug() << "not a suitable metric leader event: " << candidate;
        }
    }

    throw EventAttr::InvalidEvent{ "no suitable metric leader event found" };
}

EventResolver::EventResolver()
{
    struct predef_event
    {
        std::string name;
        perf_type_id type;
        uint64_t config;
    };
    std::vector<struct predef_event> predef_events = {
        { "cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES },
        { "instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS },
        { "cache-references", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES },
        { "cache-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES },
        { "branch-instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS },
        { "branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES },
        { "bus-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES },
#ifdef HAVE_PERF_EVENT_STALLED_CYCLES_FRONTEND
        { "stalled-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND },
#endif
#ifdef HAVE_PERF_EVENT_STALLED_CYCLES_BACKEND
        { "stalled-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND },
#endif
#ifdef HAVE_PERF_EVENT_REF_CYCLES
        { "ref-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES },
        { "cpu-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK },
        { "task-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK },
        { "page-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS },
        { "context-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES },
        { "cpu-migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS },
        { "minor-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN },
        { "major-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ },
#ifdef HAVE_PERF_EVENT_ALIGNMENT_FAULTS
        { "alignment-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS },
#endif
#ifdef HAVE_PERF_EVENT_EMULATION_FAULTS
        { "emulation-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS },
#endif
#ifdef HAVE_PERF_EVENT_DUMMY
        { "dummy", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_DUMMY },
#endif
#ifdef HAVE_PERF_EVENT_BPF_OUTPUT
        { "bpf-output", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT },
#endif
#ifdef HAVE_PERF_EVENT_CGROUP_SWITCHES
        { "cgroup-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES },
#endif
    };

    for (auto& predef_ev : predef_events)
    {
        try
        {
            PredefinedEventAttr ev(predef_ev.name, predef_ev.type, predef_ev.config);
            event_map_.emplace(predef_ev.name, ev);
        }
        catch (EventAttr::InvalidEvent& e)
        {
            continue;
        }
    }
#endif
    std::stringstream name_fmt;
    for (auto& cache : CACHE_NAME_TABLE)
    {
        for (auto& operation : CACHE_OPERATION_TABLE)
        {
            try
            {
                name_fmt.str(std::string());
                name_fmt << cache.name << '-' << operation.name;

                event_map_.emplace(
                    name_fmt.str(),
                    PredefinedEventAttr(
                        name_fmt.str(), PERF_TYPE_HW_CACHE,
                        make_cache_config(cache.id, operation.id.op_id, operation.id.result_id)));
            }
            catch (EventAttr::InvalidEvent& e)
            {
            }
        }
    }
}

EventAttr EventResolver::cache_event(const std::string& name)
{
    // Format for raw events is r followed by a hexadecimal number
    static const std::regex raw_regex("r[[:xdigit:]]{1,8}");

    // save event in event map; return a reference to the inserted event to
    // the caller.
    try
    {
        if (regex_match(name, raw_regex))
        {
            std::optional<EventAttr> ev = RawEventAttr(name);
            return event_map_.emplace(name, ev).first->second.value();
        }
        else
        {
            std::optional<EventAttr> ev = SysfsEventAttr(name);
            return event_map_.emplace(name, ev).first->second.value();
        }
    }
    catch (const EventAttr::InvalidEvent& e)
    {
        event_map_.emplace(name, std::nullopt);
        throw e;
    }
}

/**
 * takes the name of an event and looks it up in an internal event list.
 * @returns The corresponding PerfEvent if it is available
 * @throws InvalidEvent if the event is unavailable
 */
EventAttr EventResolver::get_event_by_name(const std::string& name)
{
    auto event_it = event_map_.find(name);
    if (event_it != event_map_.end())
    {
        if (event_it->second.has_value())
        {
            return event_it->second.value();
        }
        else
        {
            throw EventAttr::InvalidEvent("The event '" + name + "' is not available");
        }
    }
    else
    {
        return cache_event(name);
    }
}

EventAttr EventResolver::get_metric_leader(const std::string& metric_leader)
{
    std::optional<EventAttr> leader;
    Log::info() << "choosing default metric-leader";
    if (metric_leader == "")
    {

        try
        {
            leader = EventResolver::instance().get_event_by_name("cpu-clock");
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            Log::warn() << "cpu-clock isn't available, trying to use a fallback event";
            try
            {
                leader = EventResolver::instance().fallback_metric_leader_event();
            }
            catch (const EventAttr::InvalidEvent& e)
            {
                Log::error() << "Failed to determine a suitable metric leader event";
                Log::error() << "Try manually specifying one with --metric-leader.";
            }
        }
    }
    else
    {
        try
        {
            leader = perf::EventResolver::instance().get_event_by_name(metric_leader);
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            Log::error() << "Metric leader " << metric_leader << " not available.";
            Log::error() << "Please choose another metric leader.";
        }
    }
    return leader.value();
}

bool EventResolver::has_event(const std::string& name)
{
    const auto event_it = event_map_.find(name);
    if (event_it != event_map_.end())
    {
        return (event_it->second.has_value());
    }
    else
    {
        try
        {
            cache_event(name);
            return true;
        }
        catch (const EventAttr::InvalidEvent&)
        {
            return false;
        }
    }
}

std::vector<EventAttr> EventResolver::get_predefined_events()
{
    std::vector<EventAttr> events;
    events.reserve(event_map_.size());

    for (const auto& event : event_map_)
    {
        if (event.second.has_value())
        {
            events.push_back(std::move(event.second.value()));
        }
    }

    return events;
}

} // namespace perf
} // namespace lo2s
