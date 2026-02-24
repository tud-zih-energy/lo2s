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
#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <cstdint>

#include <fmt/format.h>
#ifdef HAVE_LIBPFM
#include <lo2s/perf/pfm.hpp>
#endif
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/pmu-events.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <regex>
#include <sstream>
#include <vector>

#include <cstring>

extern "C"
{
#include <linux/perf_event.h>
#include <linux/version.h>
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

string_to_id<perf_hw_cache_id> CACHE_NAME_TABLE[] = {
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

string_to_id<cache_op_and_result> CACHE_OPERATION_TABLE[] = {
    { "loads", { PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "stores", { PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "prefetches", { PERF_COUNT_HW_CACHE_OP_PREFETCH, PERF_COUNT_HW_CACHE_RESULT_ACCESS } },
    { "load-misses", { PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS } },
    { "store-misses", { PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_MISS } },
    { "prefetch-misses", { PERF_COUNT_HW_CACHE_OP_PREFETCH, PERF_COUNT_HW_CACHE_RESULT_MISS } },
};

constexpr std::uint64_t make_cache_config(perf_hw_cache_id cache, perf_hw_cache_op_id op,
                                          perf_hw_cache_op_result_id op_result)
{
    return cache | (op << 8) | (op_result << 16);
}

template <std::size_t N, typename T>
constexpr std::size_t array_size(T (& /*unused*/)[N])
{
    return N;
}
} // namespace

namespace lo2s::perf
{

std::vector<SysfsEventAttr> EventResolver::get_pmu_events()
{
    std::vector<SysfsEventAttr> events;

    const std::filesystem::path pmu_devices("/sys/bus/event_source/devices");

    for (const auto& pmu : std::filesystem::directory_iterator(pmu_devices))
    {
        const std::filesystem::path event_dir(pmu.path() / "events");

        // some PMUs don't have any events, in that case event_dir doesn't exist
        if (!std::filesystem::is_directory(event_dir))
        {
            continue;
        }

        for (const auto& event : std::filesystem::directory_iterator(event_dir))
        {
            std::stringstream event_name;

            const auto extension = event.path().extension();

            // ignore scaling and unit information
            if (extension == ".scale" || extension == ".unit")
            {
                continue;
            }

            // use std::filesystem::path::string, otherwise the paths are formatted quoted
            event_name << pmu.path().filename().string() << '/' << event.path().filename().string()
                       << '/';
            try
            {
                SysfsEventAttr const event(event_name.str());
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

        ifs_available_events.open("/sys/kernel/tracing/available_events");
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
    for (const auto* candidate : {
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
            name_fmt.str(std::string());
            name_fmt << cache.name << '-' << operation.name;

            try
            {
                event_map_.emplace(
                    name_fmt.str(),
                    PredefinedEventAttr(
                        name_fmt.str(), PERF_TYPE_HW_CACHE,
                        make_cache_config(cache.id, operation.id.op_id, operation.id.result_id)));
            }
            catch (EventAttr::InvalidEvent& e)
            {
                Log::trace() << "Can not insert cache event '" << name_fmt.str()
                             << "' : " << e.what();
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

    std::optional<EventAttr> res;
    if (regex_match(name, raw_regex))
    {
        res = event_map_.emplace(name, RawEventAttr(name)).first->second;
    }
#ifdef HAVE_LIBPFM
    try
    {
        res = event_map_.emplace(name, PFM4::instance().pfm4_read_event(name)).first->second;
    }
    catch (EventAttr::InvalidEvent& e)
    {
        Log::trace() << "Can not open '" << name << "' as a libpfm4 event";
    }
#endif
    if (!res.has_value())
    {
        try
        {
            res = event_map_.emplace(name, PMUEvents::instance().read_event(name)).first->second;
        }
        catch (EventAttr::InvalidEvent& e)
        {
            Log::trace() << "Can not open '" << name << "' as a pmu-events event";
        }
    }

    if (!res.has_value())
    {
        try
        {
            res = event_map_.emplace(name, SysfsEventAttr(name)).first->second;
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            event_map_.emplace(name, std::nullopt);
            throw std::runtime_error(
                fmt::format("Could not parse '{}' as a perf event: {}", name, e.what()));
        }
    }
    if (!res.has_value())
    {
        throw std::runtime_error(
            fmt::format("Trying to cache_event '{}' event, but it already exists in the event "
                        "map!. This is a bug, please report it to the developers!",
                        name));
    }

    return res.value();
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
        throw EventAttr::InvalidEvent("The event '" + name + "' is not available");
    }
    return cache_event(name);
}

EventAttr EventResolver::get_metric_leader(const std::string& metric_leader)
{
    std::optional<EventAttr> leader;
    Log::info() << "choosing default metric-leader";
    if (metric_leader.empty())
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

std::vector<EventAttr> EventResolver::get_predefined_events()
{
    std::vector<EventAttr> events;
    events.reserve(event_map_.size());

    for (const auto& event : event_map_)
    {
        if (event.second.has_value())
        {
            events.push_back(event.second.value());
        }
    }

    return events;
}

} // namespace lo2s::perf
