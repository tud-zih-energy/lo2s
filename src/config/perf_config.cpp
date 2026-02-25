#include <lo2s/config/perf_config.hpp>

#include <lo2s/config/perf/block_io_config.hpp>
#include <lo2s/config/perf/group_config.hpp>
#include <lo2s/config/perf/posix_io_config.hpp>
#include <lo2s/config/perf/sampling_config.hpp>
#include <lo2s/config/perf/syscall_config.hpp>
#include <lo2s/config/perf/tracepoint_config.hpp>
#include <lo2s/config/perf/userspace_config.hpp>
#include <lo2s/io.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/pfm.hpp>
#include <lo2s/perf/pmu-events.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/util.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdlib>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace lo2s::perf
{

namespace
{
void print_availability(std::ostream& os, const std::string& description,
                        const std::vector<perf::EventAttr>& events)
{
    std::vector<std::string> event_names;
    for (const auto& ev : events)
    {
        std::string availability;
        std::string cpu;

        if (ev.availability() == perf::Availability::PROCESS_MODE)
        {
            availability = " *";
        }
        else if (ev.availability() == perf::Availability::SYSTEM_MODE)
        {
            availability = " #";

            if (ev.supported_cpus() != Topology::instance().cpus())
            {
                const auto& cpus = ev.supported_cpus();

                std::vector<int> cpus_int;
                std::transform(cpus.begin(), cpus.end(), std::back_inserter(cpus_int),
                               [](Cpu cpu) { return cpu.as_int(); });

                if (cpus_int.size() == 1)
                {
                    cpu = fmt::format(" [ CPU {} ]", *cpus_int.begin());
                }
                else
                {
                    int min_cpu = *std::min_element(cpus_int.begin(), cpus_int.end());
                    int max_cpu = *std::max_element(cpus_int.begin(), cpus_int.end());

                    if (max_cpu - min_cpu + 1 == static_cast<int>(cpus_int.size()))
                    {
                        cpu = fmt::format(" [ CPUs {}-{} ]", min_cpu, max_cpu);
                    }
                    else
                    {
                        cpu = fmt::format(" [ CPUs {}]", fmt::join(cpus_int, ", "));
                    }
                }
            }
        }

        event_names.push_back(ev.name() + availability + cpu);
    }
    list_arguments_sorted(os, description, event_names);
}
} // namespace

void Config::parse_print_options(nitro::options::arguments& arguments)
{
    TracepointConfig::parse_print_options(arguments);
    if (arguments.given("list-events"))
    {
        print_availability(std::cout, "predefined events",
                           perf::EventResolver::instance().get_predefined_events());
        // TODO: find a better solution ?
        std::vector<perf::SysfsEventAttr> sys_events =
            perf::EventResolver::instance().get_pmu_events();
        const std::vector<perf::EventAttr> events(sys_events.begin(), sys_events.end());
        print_availability(std::cout, "Kernel PMU events", events);

#ifdef HAVE_LIBPFM
        print_availability(std::cout, "Libpfm events", perf::PFM4::instance().get_pfm4_events());
#endif

        print_availability(std::cout, "perf pmu-events",
                           perf::PMUEvents::instance().get_pmu_events());
        std::cout << "(* Only available in process-monitoring mode" << std::endl;
        std::cout << "(# Only available in system-monitoring mode" << std::endl;

        std::exit(EXIT_SUCCESS);
    }
    if (arguments.given("list-clockids"))
    {
        const auto& clockids = time::ClockProvider::get_descriptions();
        std::cout << io::make_argument_list("available clockids", std::begin(clockids),
                                            std::end(clockids));
        std::exit(EXIT_SUCCESS);
    }
}

void Config::add_parser(nitro::options::parser& parser)
{
    auto& perf_options = parser.group("General perf options");

    perf_options.option("clockid", "Reference clock used as timestamp source.")
        .short_name("k")
        .default_value("monotonic-raw")
        .metavar("CLOCKID");

    perf_options.option("mmap-pages", "Number of pages to be used by internal buffers.")
        .short_name("m")
        .default_value("16")
        .metavar("PAGES");
    perf_options
        .option("cgroup",
                "Only record perf events for the given cgroup. Can only be used in system-mode")
        .metavar("NAME")
        .optional();
    perf_options.toggle("list-events", "List available metric and sampling events.");
    perf_options.toggle("list-clockids", "List all available clockids.");

    SamplingConfig::add_parser(parser);
    TracepointConfig::add_parser(parser);
    BlockIOConfig::add_parser(parser);
    PosixIOConfig::add_parser(parser);
    SyscallConfig::add_parser(parser);

    counter::UserspaceConfig::add_parser(parser);
    counter::GroupConfig::add_parser(parser);
}

Config::Config(nitro::options::arguments& arguments)
: sampling({ arguments }), tracepoints({ arguments }), block_io({ arguments }),
  posix_io({ arguments }), syscall({ arguments }), userspace({ arguments }), group({ arguments })
{
    try
    {
        std::string requested_clock_name = arguments.get("clockid");
        // large PEBS only works when the clockid isn't set, however the internal clock
        // large PEBS uses should be equal to monotonic-raw, so use monotonic-raw outside
        // of pebs and everything should be in sync
        if (requested_clock_name == "pebs")
        {
            requested_clock_name = "monotonic-raw";
            sampling.use_pebs = true;
        }
        const auto& clock = lo2s::time::ClockProvider::get_clock_by_name(requested_clock_name);

        lo2s::Log::debug() << "Using clock \'" << clock.name << "\'.";
#ifndef USE_HW_BREAKPOINT_COMPAT
        clockid = clock.id;
#else
        if (requested_clock_name != "monotonic-raw")
        {
            lo2s::Log::warn() << "This installation was built without support for setting a "
                                 "perf reference clock.";
            lo2s::Log::warn() << "Any parameter to -k/--clockid will only affect the "
                                 "local reference clock.";
        }
#endif
        lo2s::time::Clock::set_clock(clock.id);

        if (arguments.provided("cgroup"))
        {
            cgroup_fd = get_cgroup_mountpoint_fd(arguments.get("cgroup"));

            if (cgroup_fd == -1)
            {
                Log::fatal() << "Can not open cgroup directory for " << arguments.get("cgroup");
                std::exit(EXIT_FAILURE);
            }
        }

        mmap_pages = arguments.as<std::size_t>("mmap-pages");
    }
    catch (const lo2s::time::ClockProvider::InvalidClock& e)
    {
        lo2s::Log::fatal() << "Invalid clock requested: " << e.what();
        std::exit(EXIT_FAILURE);
    }
}

void Config::check()
{
    if (any_perf())
    {
        if (perf::perf_event_paranoid() == 3)
        {
            std::cerr << "kernel.perf_event_paranoid is set to 3, which disables perf altogether."
                      << std::endl;
            std::cerr << "To solve this error, you can do one of the following:" << std::endl;
            std::cerr << " * sysctl kernel.perf_event_paranoid=2" << std::endl;
            std::cerr << " * run lo2s as root" << std::endl;

            std::exit(1);
        }
        if (any_tracepoints())
        {
            try
            {
                if (!std::filesystem::exists("/sys/kernel/tracing"))
                {
                    Log::error() << "syscall, block-io and tracepoint recording require access to "
                                    "/sys/kernel/tracing, make sure it exists and is accessible";
                    std::exit(EXIT_FAILURE);
                }
            }
            catch (std::filesystem::filesystem_error&)
            {
                Log::error() << "syscall, block-io and tracepoint recording require access to "
                                "/sys/kernel/tracing, make sure it exists and is accessible";
                std::exit(EXIT_FAILURE);
            }
        }
    }
    for (const auto& event : userspace.counters)
    {
        auto it = std::find(group.counters.begin(), group.counters.end(), event);

        if (it != group.counters.end())
        {
            Log::warn()
                << event
                << " given as both userspace and grouped metric event only using it in userspace "
                   "measuring mode";
            group.counters.erase(it);
        }

        group.check();
    }
}

void to_json(nlohmann::json& j, const perf::Config& config)
{
    j = nlohmann::json({ { "sampling", config.sampling },
                         { "tracepoints", config.tracepoints },
                         { "block_io", config.block_io },
                         { "posix_io", config.posix_io },
                         { "use_perf", config.any_perf() },
                         { "use_tracepoints", config.any_tracepoints() },
                         { "syscall", config.syscall },
                         { "group", config.group },
                         { "userspace", config.userspace } });
}
} // namespace lo2s::perf
