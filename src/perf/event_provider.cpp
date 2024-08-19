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
#include <lo2s/perf/pmu-events.hpp>
#include <lo2s/perf/event_description.hpp>
#include <lo2s/perf/event_provider.hpp>
#ifdef HAVE_LIBPFM
#include <lo2s/perf/pfm.hpp>
#endif
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
#define PERF_EVENT_HW(name, id) PERF_EVENT(name, PERF_TYPE_HARDWARE, PERF_COUNT_HW_##id)
#define PERF_EVENT_SW(name, id) PERF_EVENT(name, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_##id)

static lo2s::perf::EventDescription HW_EVENT_TABLE[] = {
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

static lo2s::perf::EventDescription SW_EVENT_TABLE[] = {
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

const EventDescription sysfs_read_event(const std::string& ev_desc);

bool event_is_openable(EventDescription& ev)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = ev.type;
    attr.config = ev.config;
    attr.config1 = ev.config1;

    int proc_fd = perf_event_open(&attr, ExecutionScope(Thread(0)), -1, 0);
    int sys_fd = perf_event_open(&attr, ExecutionScope(*ev.supported_cpus().begin()), -1, 0);
    if (sys_fd == -1 && proc_fd == -1)
    {
        Log::debug() << "perf event not openable, retrying with exclude_kernel=1";

        attr.exclude_kernel = 1;
        int proc_fd = perf_event_open(&attr, ExecutionScope(Thread(0)), -1, 0);
        int sys_fd = perf_event_open(&attr, ExecutionScope(*ev.supported_cpus().begin()), -1, 0);

        if (sys_fd == -1 && proc_fd == -1)
        {
            switch (errno)
            {
            case ENOTSUP:
                Log::debug() << "perf event not supported by the running kernel: " << ev.name;
                break;
            default:
                Log::debug() << "perf event " << ev.name
                             << " not available: " << std::string(std::strerror(errno));
                break;
            }
            return false;
        }
    }
    if (sys_fd == -1 && proc_fd != -1)
    {
        close(proc_fd);
        ev.availability = Availability::PROCESS_MODE;
    }
    else if (proc_fd == -1 && sys_fd != -1)
    {
        close(sys_fd);
        ev.availability = Availability::SYSTEM_MODE;
    }
    else
    {
        ev.availability = Availability::UNIVERSAL;
        close(sys_fd);
        close(proc_fd);
    }

    return true;
}

static void populate_event_map(EventProvider::EventMap& map)
{
    Log::info() << "checking available events...";
    map.reserve(array_size(HW_EVENT_TABLE) + array_size(SW_EVENT_TABLE) +
                array_size(CACHE_NAME_TABLE) * array_size(CACHE_OPERATION_TABLE));
    for (auto& ev : HW_EVENT_TABLE)
    {
        map.emplace(ev.name,
                    event_is_openable(ev) ? ev : EventProvider::DescriptionCache::make_invalid());
    }

    for (auto& ev : SW_EVENT_TABLE)
    {
        map.emplace(ev.name,
                    event_is_openable(ev) ? ev : EventProvider::DescriptionCache::make_invalid());
    }

    std::stringstream name_fmt;
    for (auto& cache : CACHE_NAME_TABLE)
    {
        for (auto& operation : CACHE_OPERATION_TABLE)
        {
            name_fmt.str(std::string());
            name_fmt << cache.name << '-' << operation.name;

            EventDescription ev(
                name_fmt.str(), PERF_TYPE_HW_CACHE,
                make_cache_config(cache.id, operation.id.op_id, operation.id.result_id));

            map.emplace(ev.name, event_is_openable(ev) ?
                                     ev :
                                     EventProvider::DescriptionCache::make_invalid());
        }
    }
}

std::vector<EventDescription> EventProvider::get_pmu_events()
{
    std::vector<EventDescription> events;

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
                events.emplace_back(sysfs_read_event(event_name.str()));
            }
            catch (const EventProvider::InvalidEvent& e)
            {
                Log::debug() << "Can not open event " << event_name.str() << ":" << e.what();
            }
        }
    }

    return events;
}

EventDescription EventProvider::fallback_metric_leader_event()
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
            const EventDescription ev = get_event_by_name(candidate);
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

const EventDescription raw_read_event(const std::string& ev_desc)
{
    uint64_t code = std::stoull(ev_desc.substr(1), nullptr, 16);

    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = PERF_TYPE_RAW;
    perf_attr.config = code;
    perf_attr.config1 = 0;
    perf_attr.exclude_kernel = 1;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    std::set<Cpu> cpus;
    for (const auto& cpu : Topology::instance().cpus())
    {
        int fd = perf_try_event_open(&perf_attr, cpu.as_scope(), -1, 0, -1);
        if (fd != -1)
        {
            cpus.emplace(cpu);
            close(fd);
        }
    }

    const EventDescription event(ev_desc, PERF_TYPE_RAW, code, 0, cpus);
    // Do not check whether the event_is_openable because we don't know whether we are in
    // system or process mode
    return event;
}

const EventDescription sysfs_read_event(const std::string& ev_desc)
{
    // Parse event description //

    /* Event description format:
     *   Name of a Performance Monitoring Unit (PMU) and an event name,
     *   separated by either '/' or ':' (for perf-like syntax);  followed by an
     *   optional separator:
     *
     *     <pmu>/<event_name>[/]
     *   OR
     *     <pmu>:<event_name>[/]
     *
     *   Examples (both specify the same event):
     *
     *     cpu/cache-misses/
     *     cpu:cache-misses
     *
     * */

    enum EVENT_DESCRIPTION_REGEX_GROUPS
    {
        ED_WHOLE_MATCH,
        ED_PMU,
        ED_NAME,
    };

    static const std::regex ev_desc_regex(R"(([a-z0-9-_]+)[\/:]([a-z0-9-_]+)\/?)");
    std::smatch ev_desc_match;

    if (!std::regex_match(ev_desc, ev_desc_match, ev_desc_regex))
    {
        throw EventProvider::InvalidEvent("invalid event description format");
    }

    const std::string& pmu_name = ev_desc_match[ED_PMU];
    const std::string& event_name = ev_desc_match[ED_NAME];

    Log::debug() << "parsing event description: pmu='" << pmu_name << "', event='" << event_name
                 << "'";

    const std::filesystem::path pmu_path =
        std::filesystem::path("/sys/bus/event_source/devices") / pmu_name;

    // read PMU type id
    std::underlying_type<perf_type_id>::type type;
    std::ifstream type_stream(pmu_path / "type");
    type_stream >> type;
    if (!type_stream)
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown PMU '"s + pmu_name + "'");
    }

    // If the processor is heterogenous, "cpus" contains the cores that support this PMU. If the PMU
    // is an uncore PMU "cpumask" contains the cores that are logically assigned to that PMU. Why
    // there need to be two seperate files instead of one, nobody knows, but simply parse both.
    std::set<Cpu> cpus;
    auto cpuids = parse_list_from_file(pmu_path / "cpus");

    if (cpuids.empty())
    {
        cpuids = parse_list_from_file(pmu_path / "cpumask");
    }

    std::transform(cpuids.begin(), cpuids.end(), std::inserter(cpus, cpus.end()),
                   [](uint32_t cpuid) { return Cpu(cpuid); });
    EventDescription event(ev_desc, static_cast<perf_type_id>(type), 0, 0, cpus);

    // Parse event configuration from sysfs //

    // read event configuration
    std::string ev_cfg;
    std::filesystem::path event_path = pmu_path / "events" / event_name;
    std::ifstream event_stream(event_path);
    event_stream >> ev_cfg;
    if (!event_stream)
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown event '"s + event_name + "' for PMU '"s +
                                          pmu_name + "'");
    }

    parse_event_configuration(event, pmu_path, ev_cfg);
    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu_name << "/"
                 << event_name << "/type=" << event.type << ",config=" << event.config
                 << ",config1=" << event.config1 << std::dec << std::noshowbase << "/";

    std::ifstream scale_stream(event_path.string() + ".scale");
    scale_stream >> event.scale;
    if (scale_stream.fail())
    {
        event.scale = 1;
    }

    std::ifstream unit_stream(event_path.string() + ".unit");
    unit_stream >> event.unit;
    if (scale_stream.fail())
    {
        event.unit = "#";
    }

    if (!event_is_openable(event))
    {
        throw EventProvider::InvalidEvent(
            "Event can not be opened in process- or system-monitoring-mode");
    }
    return event;
}

EventProvider::EventProvider()
{
    populate_event_map(event_map_);
}

EventDescription EventProvider::cache_event(const std::string& name)
{
    // Format for raw events is r followed by a hexadecimal number
    static const std::regex raw_regex("r[[:xdigit:]]{1,8}");

    // save event in event map; return a reference to the inserted event to
    // the caller.
    try
    {
        if (regex_match(name, raw_regex))
        {
            return event_map_.emplace(name, DescriptionCache(raw_read_event(name)))
                .first->second.description;
        }
        else
        {
            return event_map_.emplace(name, DescriptionCache(sysfs_read_event(name)))
                .first->second.description;
        }
    }
    catch (const InvalidEvent& e)
    {
#ifdef HAVE_LIBPFM
        auto ev = PFM4::instance().pfm4_read_event(name);
        if (!ev)
        {
            auto ev = pmu_events::read_event(name);
            if(!ev)
            {
                event_map_.emplace(name, DescriptionCache::make_invalid());
                throw e;
            }
        }

        return *ev;
#else
        event_map_.emplace(name, DescriptionCache::make_invalid());
        throw e;
#endif
    }
}

EventDescription EventProvider::get_event_by_name(const std::string& name)
{
    auto& ev_map = instance().event_map_;
    auto event_it = ev_map.find(name);
    if (event_it != ev_map.end())
    {
        if (event_it->second.is_valid())
        {
            return event_it->second.description;
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
        return event_it->second.is_valid();
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

std::vector<EventDescription> EventProvider::get_predefined_events()
{

    const auto& ev_map = instance().event_map_;

    std::vector<EventDescription> events;
    events.reserve(ev_map.size());

    for (const auto& event : ev_map)
    {
        if (event.second.is_valid())
        {
            events.push_back(event.second.description);
        }
    }

    return events;
}
} // namespace perf
} // namespace lo2s
