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
#define PERF_EVENT(name, type, id) { (name), (type), (id) }
#define PERF_EVENT_HW(name, id) PERF_EVENT(name, PERF_TYPE_HARDWARE, PERF_COUNT_HW_##id)
#define PERF_EVENT_SW(name, id) PERF_EVENT(name, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_##id)

static lo2s::perf::PerfEvent HW_EVENT_TABLE[] = {
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

static lo2s::perf::PerfEvent SW_EVENT_TABLE[] = {
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

constexpr std::uint64_t operator"" _u64(unsigned long long int lit)
{
    return static_cast<std::uint64_t>(lit);
}

constexpr std::uint64_t bit(int bitnumber)
{
    return static_cast<std::uint64_t>(1_u64 << bitnumber);
}
} // namespace

namespace lo2s
{
namespace perf
{

const SysfsEvent sysfs_read_event(const std::string& ev_name);

static bool event_is_openable(SysfsEvent& provider)
{
    provider.set_availability();

    if (!provider.is_valid())
    {
        Log::debug() << "perf event not openable, retrying with exclude_kernel=1";
        provider.get_attr().exclude_kernel = 1;
        provider.set_availability();

        if (!provider.is_valid())
        {
            switch (errno)
            {
            case ENOTSUP:
                Log::debug() << "perf event not supported by the running kernel: "
                             << provider.get_name();
                break;
            default:
                Log::debug() << "perf event " << provider.get_name()
                             << " not available: " << std::string(std::strerror(errno));
                break;
            }
            return false;
        }
    }
    return true;
}

static void populate_event_map(std::unordered_map<std::string, PerfEvent>& map)
{
    Log::info() << "checking available events...";
    map.reserve(array_size(HW_EVENT_TABLE) + array_size(SW_EVENT_TABLE) +
                array_size(CACHE_NAME_TABLE) * array_size(CACHE_OPERATION_TABLE));
    for (auto& ev : HW_EVENT_TABLE)
    {
        PerfEvent event(ev);
        map.emplace(event.get_name(), event);
    }

    for (auto& ev : SW_EVENT_TABLE)
    {
        PerfEvent event(ev);
        map.emplace(event.get_name(), event);
    }

    std::stringstream name_fmt;
    for (auto& cache : CACHE_NAME_TABLE)
    {
        for (auto& operation : CACHE_OPERATION_TABLE)
        {
            name_fmt.str(std::string());
            name_fmt << cache.name << '-' << operation.name;

            map.emplace(name_fmt.str(), PerfEvent(name_fmt.str(), PERF_TYPE_HW_CACHE,
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

PerfEvent EventProvider::fallback_metric_leader_event()
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
            const PerfEvent ev = get_event_by_name(candidate);
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

static std::uint64_t parse_bitmask(const std::string& format)
{
    enum BITMASK_REGEX_GROUPS
    {
        BM_WHOLE_MATCH,
        BM_BEGIN,
        BM_END,
    };

    std::uint64_t mask = 0x0;

    static const std::regex bit_mask_regex(R"((\d+)?(?:-(\d+)))");
    const std::sregex_iterator end;
    std::smatch bit_mask_match;
    for (std::sregex_iterator i = { format.begin(), format.end(), bit_mask_regex }; i != end; ++i)
    {
        const auto& match = *i;
        int start = std::stoi(match[BM_BEGIN]);
        int end = (match[BM_END].length() == 0) ? start : std::stoi(match[BM_END]);

        const auto len = (end + 1) - start;
        if (start < 0 || end > 63 || len > 64)
        {
            throw EventProvider::InvalidEvent("invalid config mask");
        }

        /* Set `len` bits and shift them to where they should start.
         * 4-bit example: format "1-3" produces mask 0b1110.
         *    start := 1, end := 3
         *    len  := 3 + 1 - 1 = 3
         *    bits := bit(3) - 1 = 0b1000 - 1 = 0b0111
         *    mask := 0b0111 << 1 = 0b1110
         * */

        // Shifting by 64 bits causes undefined behaviour, so in this case set
        // all bits by assigning the maximum possible value for std::uint64_t.
        const std::uint64_t bits =
            (len == 64) ? std::numeric_limits<std::uint64_t>::max() : bit(len) - 1;

        mask |= bits << start;
    }
    Log::debug() << std::showbase << std::hex << "config mask: " << format << " = " << mask
                 << std::dec << std::noshowbase;
    return mask;
}

static constexpr std::uint64_t apply_mask(std::uint64_t value, std::uint64_t mask)
{
    std::uint64_t res = 0;
    for (int mask_bit = 0, value_bit = 0; mask_bit < 64; mask_bit++)
    {
        if (mask & bit(mask_bit))
        {
            res |= ((value >> value_bit) & bit(0)) << mask_bit;
            value_bit++;
        }
    }
    return res;
}

static void event_attr_update(PerfEvent& event, std::uint64_t value, const std::string& format)
{
    // Parse config terms //

    /* Format:  <term>:<bitmask>
     *
     * We only assign the terms 'config' and 'config1'.
     *
     * */

    static constexpr auto npos = std::string::npos;
    const auto colon = format.find_first_of(':');
    if (colon == npos)
    {
        throw EventProvider::InvalidEvent("invalid format description: missing colon");
    }

    const auto target_config = format.substr(0, colon);
    const auto mask = parse_bitmask(format.substr(colon + 1));

    if (target_config == "config")
    {
        event.get_attr().config |= apply_mask(value, mask);
    }

    if (target_config == "config1")
    {
        event.get_attr().config1 |= apply_mask(value, mask);
    }
}

/**
 * takes the name of an event, checks if it can be opened with each cpu and returns a PerfEvent
 * with a set of working cpus
 */
const PerfEvent raw_read_event(const std::string& ev_name)
{
    uint64_t code = std::stoull(ev_name.substr(1), nullptr, 16);

    PerfEvent ev(0);
    ev.get_attr().config = code;

    std::set<Cpu> cpus;
    for (const auto& cpu : Topology::instance().cpus())
    {
        try
        {
            PerfEventGuard ev_instance = ev.open(cpu, -1);
            cpus.emplace(cpu);
        }
        catch (const std::system_error& e)
        {
        }
    }

    const PerfEvent event(ev_name, PERF_TYPE_RAW, code, 0, cpus);
    // Do not check whether the event_is_openable because we don't know whether we are in
    // system or process mode
    return event;
}

template <typename T>
T read_file_or_else(std::string filename, T or_else)
{
    T val;
    std::ifstream stream(filename);
    stream >> val;
    if (stream.fail())
    {
        return or_else;
    }
    return val;
}

const SysfsEvent sysfs_read_event(const std::string& ev_name)
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

    static const std::regex ev_name_regex(R"(([a-z0-9-_]+)[\/:]([a-z0-9-_]+)\/?)");
    std::smatch ev_name_match;

    if (!std::regex_match(ev_name, ev_name_match, ev_name_regex))
    {
        throw EventProvider::InvalidEvent("invalid event description format");
    }

    const std::string& pmu_name = ev_name_match[ED_PMU];
    const std::string& event_name = ev_name_match[ED_NAME];

    Log::debug() << "parsing event description: pmu='" << pmu_name << "', event='" << event_name
                 << "'";

    const std::filesystem::path pmu_path =
        std::filesystem::path("/sys/bus/event_source/devices") / pmu_name;

    // read PMU type id
    std::underlying_type<perf_type_id>::type type = read_file_or_else(pmu_path / "type", 0);
    if (!type)
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

    SysfsEvent event(ev_name, static_cast<perf_type_id>(type), 0, 0, cpus);

    // Parse event configuration from sysfs //

    // read event configuration
    std::filesystem::path event_path = pmu_path / "events" / event_name;
    std::string ev_cfg = read_file_or_else(event_path, std::string("0"));
    if (ev_cfg == "0")
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown event '"s + event_name + "' for PMU '"s +
                                          pmu_name + "'");
    }

    /* Event configuration format:
     *   One or more terms with optional values, separated by ','.  (Taken from
     *   linux/Documentation/ABI/testing/sysfs-bus-event_source-devices-events):
     *
     *     <term>[=<value>][,<term>[=<value>]...]
     *
     *   Example (config for 'cpu/cache-misses' on an Intel Core i5-7200U):
     *
     *     event=0x2e,umask=0x41
     *
     *  */

    enum EVENT_CONFIG_REGEX_GROUPS
    {
        EC_WHOLE_MATCH,
        EC_TERM,
        EC_VALUE,
    };

    static const std::regex kv_regex(R"(([^=,]+)(?:=([^,]+))?)");

    Log::debug() << "parsing event configuration: " << ev_cfg;
    std::smatch kv_match;
    while (std::regex_search(ev_cfg, kv_match, kv_regex))
    {
        static const std::string default_value("0x1");

        const std::string& term = kv_match[EC_TERM];
        const std::string& value =
            (kv_match[EC_VALUE].length() != 0) ? kv_match[EC_VALUE] : default_value;

        std::string format = read_file_or_else(pmu_path / "format" / term, std::string("0"));
        if (format == "0")
        {
            throw EventProvider::InvalidEvent("cannot read event format");
        }

        static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                      "May not convert from unsigned long to uint64_t!");

        std::uint64_t val = std::stol(value, nullptr, 0);
        Log::debug() << "parsing config assignment: " << term << " = " << std::hex << std::showbase
                     << val << std::dec << std::noshowbase;
        event_attr_update(event, val, format);

        ev_cfg = kv_match.suffix();
    }

    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu_name << "/"
                 << event_name << "/type=" << event.get_attr().type
                 << ",config=" << event.get_attr().config << ",config1=" << event.get_attr().config1
                 << std::dec << std::noshowbase << "/";

    event.set_scale(read_file_or_else<double>(event_path.replace_extension(".scale"), 1.0));
    event.set_unit(read_file_or_else<std::string>(event_path.replace_extension(".unit"), "#"));

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

PerfEvent EventProvider::cache_event(const std::string& name)
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
            return event_map_.emplace(name, sysfs_read_event(name)).first->second;
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
PerfEvent EventProvider::get_event_by_name(const std::string& name)
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

std::vector<PerfEvent> EventProvider::get_predefined_events()
{
    const auto& ev_map = instance().event_map_;

    std::vector<PerfEvent> events;
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
} // namespace perf
} // namespace lo2s
