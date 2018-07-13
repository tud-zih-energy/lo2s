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

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/counter_description.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/util.hpp>

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/iterator_range.hpp>
#include <ios>
#include <limits>
#include <regex>
#include <sstream>

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

static const lo2s::perf::CounterDescription HW_EVENT_TABLE[] = {
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

static const lo2s::perf::CounterDescription SW_EVENT_TABLE[] = {
    PERF_EVENT_SW("cpu-clock", CPU_CLOCK),
    PERF_EVENT_SW("task-clock", TASK_CLOCK),
    PERF_EVENT_SW("page-faults", PAGE_FAULTS),
    PERF_EVENT_SW("context-switches", CONTEXT_SWITCHES),
    PERF_EVENT_SW("cpu-migrations", CPU_MIGRATIONS),
    PERF_EVENT_SW("minor-faults", PAGE_FAULTS_MIN),
    PERF_EVENT_SW("major-faults", PAGE_FAULTS_MAJ),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
    PERF_EVENT_SW("alignment-faults", ALIGNMENT_FAULTS),
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

static constexpr string_to_id<perf_hw_cache_id> CACHE_NAME_TABLE[] = {
    { "L1-dcache", PERF_COUNT_HW_CACHE_L1D }, { "L1-icache", PERF_COUNT_HW_CACHE_L1I },
    { "LLC", PERF_COUNT_HW_CACHE_LL },        { "dTLB", PERF_COUNT_HW_CACHE_DTLB },
    { "iTLB", PERF_COUNT_HW_CACHE_ITLB },     { "branch", PERF_COUNT_HW_CACHE_BPU },
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0)
    { "node", PERF_COUNT_HW_CACHE_NODE },
#endif
};

struct cache_op_and_result
{
    perf_hw_cache_op_id op_id;
    perf_hw_cache_op_result_id result_id;
};

static constexpr string_to_id<cache_op_and_result> CACHE_OPERATION_TABLE[] = {
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

static bool event_is_openable(const CounterDescription& ev)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = ev.type;
    attr.config = ev.config;
    attr.config1 = ev.config1;
    attr.exclude_kernel = 0;

    int fd = perf_event_open(&attr, 0, -1, -1, 0);
    if (fd == -1)
    {
        switch (errno)
        {
        case ENOTSUP:
            Log::debug() << "perf event not supported by the running kernel: " << ev.name;
            break;
        default:
            Log::debug() << "perf event not available: " << ev.name;
            break;
        }
        return false;
    }

    close(fd);
    return true;
}

static void populate_event_map(EventProvider::EventMap& map)
{
    Log::info() << "checking available events...";
    map.reserve(array_size(HW_EVENT_TABLE) + array_size(SW_EVENT_TABLE) +
                array_size(CACHE_NAME_TABLE) * array_size(CACHE_OPERATION_TABLE));
    for (const auto& ev : HW_EVENT_TABLE)
    {
        map.emplace(ev.name,
                    event_is_openable(ev) ? ev : EventProvider::DescriptionCache::make_invalid());
    }

    for (const auto& ev : SW_EVENT_TABLE)
    {
        map.emplace(ev.name,
                    event_is_openable(ev) ? ev : EventProvider::DescriptionCache::make_invalid());
    }

    std::stringstream name_fmt;
    for (const auto& cache : CACHE_NAME_TABLE)
    {
        for (const auto& operation : CACHE_OPERATION_TABLE)
        {
            name_fmt.str(std::string());
            name_fmt << cache.name << '-' << operation.name;

            CounterDescription ev(
                name_fmt.str(), PERF_TYPE_HW_CACHE,
                make_cache_config(cache.id, operation.id.op_id, operation.id.result_id));

            map.emplace(ev.name, event_is_openable(ev) ?
                                     ev :
                                     EventProvider::DescriptionCache::make_invalid());
        }
    }
}

std::vector<std::string> EventProvider::get_pmu_event_names()
{
    std::vector<std::string> names;

    namespace fs = boost::filesystem;

    const fs::path pmu_devices("/sys/bus/event_source/devices");

    for (const auto& pmu : boost::make_iterator_range(fs::directory_iterator{ pmu_devices }, {}))
    {
        const auto pmu_path = pmu.path();

        const fs::path event_dir(pmu_path / "events");

        // some PMUs don't have any events, in that case event_dir doesn't exist
        if (!fs::is_directory(event_dir))
        {
            continue;
        }

        for (const auto& event :
             boost::make_iterator_range(fs::directory_iterator{ event_dir }, {}))
        {
            std::stringstream event_name;

            const auto event_path = event.path();
            const auto extension = event_path.extension();

            // ignore scaling and unit information
            if (extension == ".scale" || extension == ".unit")
            {
                continue;
            }

            // use fs::path::string, otherwise the paths are formatted quoted
            event_name << pmu_path.filename().string() << '/' << event_path.filename().string()
                       << '/';
            names.emplace_back(event_name.str());
        }
    }

    return names;
}

const CounterDescription& EventProvider::get_default_metric_leader_event()
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
            const CounterDescription& ev = get_event_by_name(candidate);
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

static void event_description_update(CounterDescription& event, std::uint64_t value,
                                     const std::string& format)
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
        event.config |= apply_mask(value, mask);
    }

    if (target_config == "config1")
    {
        event.config1 |= apply_mask(value, mask);
    }
}

const CounterDescription raw_read_event(const std::string& ev_desc)
{
    uint64_t code = std::stoull(ev_desc.substr(1), nullptr, 16);
    const CounterDescription event(ev_desc, PERF_TYPE_RAW, code, 0);

    if (!event_is_openable(event))
    {
        throw EventProvider::InvalidEvent("cannot open requested event");
    }
    return event;
}

const CounterDescription sysfs_read_event(const std::string& ev_desc)
{
    namespace fs = boost::filesystem;

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
    static const std::regex ev_desc_regex(R"(([a-z-_]+)[\/:]([a-z-_]+)\/?)");
    std::smatch ev_desc_match;

    if (!std::regex_match(ev_desc, ev_desc_match, ev_desc_regex))
    {
        throw EventProvider::InvalidEvent("invalid event description format");
    }

    const std::string& pmu_name = ev_desc_match[ED_PMU];
    const std::string& event_name = ev_desc_match[ED_NAME];

    Log::debug() << "parsing event description: pmu='" << pmu_name << "', event='" << event_name
                 << "'";

    const fs::path pmu_path = fs::path("/sys/bus/event_source/devices") / pmu_name;

    // read PMU type id
    std::underlying_type<perf_type_id>::type type;
    if ((fs::ifstream(pmu_path / "type") >> type).fail())
    {
        using namespace std::string_literals;
        throw EventProvider::InvalidEvent("unknown PMU '"s + pmu_name + "'");
    }
    CounterDescription event(ev_desc, static_cast<perf_type_id>(type), 0, 0);

    // Parse event configuration from sysfs //

    // read event configuration
    std::string ev_cfg;
    if ((fs::ifstream(pmu_path / "events" / event_name) >> ev_cfg).fail())
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

        std::string format;
        if (!(fs::ifstream(pmu_path / "format" / term) >> format))
        {
            throw EventProvider::InvalidEvent("cannot read event format");
        }

        static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                      "May not convert from unsigned long to uint64_t!");

        std::uint64_t val = std::stol(value, nullptr, 0);
        Log::debug() << "parsing config assignment: " << term << " = " << std::hex << std::showbase
                     << val << std::dec << std::noshowbase;
        event_description_update(event, val, format);

        ev_cfg = kv_match.suffix();
    }

    Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu_name << "/"
                 << event_name << "/type=" << event.type << ",config=" << event.config
                 << ",config1=" << event.config1 << std::dec << std::noshowbase << "/";

    if (!event_is_openable(event))
    {
        throw EventProvider::InvalidEvent("cannot open requested event");
    }
    return event;
}

EventProvider::EventProvider()
{
    populate_event_map(event_map_);
}

const CounterDescription& EventProvider::cache_event(const std::string& name)
{
    // Format for raw events is rNNNN
    static const std::regex raw_regex("r[[:xdigit:]]{4}");

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
        event_map_.emplace(name, DescriptionCache::make_invalid());
        throw e;
    }
}

const CounterDescription& EventProvider::get_event_by_name(const std::string& name)
{
    auto& ev_map = instance().event_map_;
    const auto event_it = ev_map.find(name);
    if (event_it != ev_map.end())
    {
        if (event_it->second.is_valid())
        {
            return event_it->second.description;
        }
        else
        {
            throw InvalidEvent("Event failed to cache previously");
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

std::vector<std::string> EventProvider::get_predefined_event_names()
{

    const auto& ev_map = instance().event_map_;

    std::vector<EventMap::key_type> event_names;
    event_names.reserve(ev_map.size());

    for (const auto& event : ev_map)
    {
        if (event.second.is_valid())
        {
            event_names.push_back(event.first);
        }
    }

    return event_names;
}
} // namespace perf
} // namespace lo2s
