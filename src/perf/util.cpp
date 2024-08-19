#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_description.hpp>
#include <lo2s/perf/pmu-events.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/util.hpp>

extern "C"
{
#include <linux/perf_event.h>
#include <syscall.h>
}

namespace lo2s
{
namespace perf
{
int perf_event_paranoid()
{
    try
    {
        return get_sysctl<int>("kernel", "perf_event_paranoid");
    }
    catch (...)
    {
        Log::warn() << "Failed to access kernel.perf_event_paranoid. Assuming 2.";
        return 2;
    }
}

int perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                    unsigned long flags, int cgroup_fd)
{
    int cpuid = -1;
    pid_t pid = -1;
    if (scope.is_cpu())
    {
        if (cgroup_fd != -1)
        {
            pid = cgroup_fd;
            flags |= PERF_FLAG_PID_CGROUP;
        }
        cpuid = scope.as_cpu().as_int();
    }
    else
    {
        pid = scope.as_thread().as_pid_t();
    }
    return syscall(__NR_perf_event_open, perf_attr, pid, cpuid, group_fd, flags);
}

// Default options we use in every perf_event_open call
struct perf_event_attr common_perf_event_attrs()
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.size = sizeof(struct perf_event_attr);
    attr.disabled = 1;

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr.use_clockid = config().use_clockid;
    attr.clockid = config().clockid;
#endif
    // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
    // Default behaviour is to wakeup on every event, which is horrible performance wise
    attr.watermark = 1;
    attr.wakeup_watermark = static_cast<uint32_t>(0.8 * config().mmap_pages * get_page_size());

    return attr;
}

void perf_warn_paranoid()
{
    static bool warning_issued = false;

    if (!warning_issued)
    {
        Log::warn() << "You requested kernel sampling, but kernel.perf_event_paranoid > 1, "
                       "retrying without kernel samples.";
        Log::warn() << "To solve this warning you can do one of the following:";
        Log::warn() << " * sysctl kernel.perf_event_paranoid=1";
        Log::warn() << " * run lo2s as root";
        Log::warn() << " * run with --no-kernel to disable kernel space sampling in "
                       "the first place,";
        warning_issued = true;
    }
}

void perf_check_disabled()
{
    if (perf_event_paranoid() == 3)
    {
        Log::error() << "kernel.perf_event_paranoid is set to 3, which disables perf altogether.";
        Log::warn() << "To solve this error, you can do one of the following:";
        Log::warn() << " * sysctl kernel.perf_event_paranoid=2";
        Log::warn() << " * run lo2s as root";

        throw std::runtime_error("Perf is disabled via a paranoid setting of 3.");
    }
}

int perf_try_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                        unsigned long flags, int cgroup_fd)
{
    int fd = perf_event_open(perf_attr, scope, group_fd, flags, cgroup_fd);
    if (fd < 0 && errno == EACCES && !perf_attr->exclude_kernel && perf_event_paranoid() > 1)
    {
        perf_attr->exclude_kernel = 1;
        perf_warn_paranoid();
        fd = perf_event_open(perf_attr, scope, group_fd, flags, cgroup_fd);
    }
    return fd;
}

int perf_event_description_open(ExecutionScope scope, const EventDescription& desc, int group_fd)
{
    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = desc.type;
    perf_attr.config = desc.config;
    perf_attr.config1 = desc.config1;
    perf_attr.exclude_kernel = config().exclude_kernel;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

#ifndef USE_HW_BREAKPOINT_COMPAT
    perf_attr.use_clockid = config().use_clockid;
    perf_attr.clockid = config().clockid;
#endif

    int fd = perf_try_event_open(&perf_attr, scope, group_fd, 0, config().cgroup_fd);
    if (fd < 0)
    {
        Log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
    return fd;
}

constexpr std::uint64_t operator"" _u64(unsigned long long int lit)
{
    return static_cast<std::uint64_t>(lit);
}

constexpr std::uint64_t bit(int bitnumber)
{
    return static_cast<std::uint64_t>(1_u64 << bitnumber);
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
            throw std::runtime_error("invalid config mask");
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

static void event_description_update(EventDescription& event, std::uint64_t value,
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
        throw std::runtime_error("invalid format description: missing colon");
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

void parse_event_configuration(EventDescription& event, std::filesystem::path pmu_path,
                               std::string ev_cfg)
{
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
        std::ifstream format_stream(pmu_path / "format" / term);
        format_stream >> format;
        if (!format_stream)
        {
            throw std::runtime_error("cannot read event format");
        }

        static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                      "May not convert from unsigned long to uint64_t!");

        std::uint64_t val = std::stol(value, nullptr, 0);
        Log::debug() << "parsing config assignment: " << term << " = " << std::hex << std::showbase
                     << val << std::dec << std::noshowbase;
        event_description_update(event, val, format);

        ev_cfg = kv_match.suffix();
    }
}
} // namespace perf
} // namespace lo2s
