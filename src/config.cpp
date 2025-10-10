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

#include <lo2s/build_config.hpp>

#include <lo2s/io.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <nitro/options/arguments.hpp>
#ifdef HAVE_LIBPFM
#include <lo2s/perf/pfm.hpp>
#endif
#include <lo2s/perf/pmu-events.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/platform.hpp>
#include <lo2s/syscalls.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/knobs.hpp>
#endif

#include <nitro/options/parser.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime> // for CLOCK_* macros
#include <filesystem>
#include <iterator>
#include <optional>

extern "C"
{
#include <fcntl.h>
#include <unistd.h>
}

namespace lo2s
{
static inline void list_arguments_sorted(std::ostream& os, const std::string& description,
                                         std::vector<std::string> items)
{
    std::sort(items.begin(), items.end());
    os << io::make_argument_list(description, items.begin(), items.end());
}

static inline void print_availability(std::ostream& os, const std::string& description,
                                      std::vector<perf::EventAttr> events)
{
    std::vector<std::string> event_names;
    for (const auto& ev : events)
    {
        std::string availability = "";
        std::string cpu = "";

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

static inline void print_version(std::ostream& os)
{
    // clang-format off
    os << "lo2s " << lo2s::version() << "\n"
       << "Copyright (C) " LO2S_COPYRIGHT_YEAR " Technische Universitaet Dresden, Germany\n"
       << "This is free software; see the source for copying conditions.  There is NO\n"
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    // clang-format on
}

std::vector<int64_t> parse_syscall_names(std::vector<std::string> requested_syscalls)
{
    std::vector<int64_t> syscall_nrs;
    for (const auto& requested_syscall : requested_syscalls)
    {
        int syscall_nr = syscall_nr_for_name(requested_syscall);
        if (syscall_nr == -1)
        {
#ifdef HAVE_LIBAUDIT
            Log::warn() << "syscall: " << requested_syscall << " not recognized!";
#else
            Log::warn() << "lo2s was compiled without libaudit support and can thus not resolve "
                           "syscall names, try the syscall number instead";
#endif
            std::exit(EXIT_FAILURE);
        }
        syscall_nrs.emplace_back(syscall_nr);
    }
    return syscall_nrs;
}

static std::optional<Config> instance = std::nullopt;

const Config& config()
{
    return *instance;
}

static Config default_config()
{
    Config config;
    // Default to use_perf_sampling (and thus use_perf) to
    // allow perf_event_open in --list-events.
    config.use_perf_sampling = true;
    return config;
}

/* Some parts of lo2s want to read config() before we are finished parsing it,
 * e.g. the event listing code. In this case supply it with a sane default config
 */

const Config& config_or_default()
{
    static Config default_conf = default_config();

    if (instance.has_value())
    {
        return *instance;
    }

    return default_conf;
}

static void set_verbosity(lo2s::Config& config, nitro::options::arguments& arguments)
{
    if (arguments.given("quiet") && arguments.given("verbose"))
    {
        lo2s::Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        config.quiet = false;
    }
    else
    {
        if (arguments.given("quiet"))
        {
            lo2s::logging::set_min_severity_level(nitro::log::severity_level::error);
        }
        else
        {
            using sl = nitro::log::severity_level;
            switch (arguments.given("verbose"))
            {
            case 0:
                lo2s::logging::set_min_severity_level(sl::warn);
                break;
            case 1:
                lo2s::Log::info() << "Enabling log-level 'info'";
                lo2s::logging::set_min_severity_level(sl::info);
                break;
            case 2:
                lo2s::Log::info() << "Enabling log-level 'debug'";
                lo2s::logging::set_min_severity_level(sl::debug);
                break;
            case 3:
            default:
                lo2s::Log::info() << "Enabling log-level 'trace'";
                lo2s::logging::set_min_severity_level(sl::trace);
                break;
            }
        }
    }
}

static void set_general_options(lo2s::Config& config, nitro::options::arguments& arguments,
                                int argc, const char** argv)
{
    config.quiet = arguments.given("quiet");

    if (arguments.given("all-cpus") || arguments.given("all-cpus-sampling"))
    {
        config.monitor_type = MonitorType::CPU_SET;
    }
    else
    {
        config.monitor_type = MonitorType::PROCESS;
    }

    config.process =
        arguments.provided("pid") ? Process(arguments.as<pid_t>("pid")) : Process::invalid();

    config.read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("readout-interval"));

    config.lo2s_command_line = "";

    // The entire command line of lo2s, to be written to the trace
    std::vector<std::string> lo2s_command_line;
    for (int i = 0; i < argc; i++)
    {
        lo2s_command_line.emplace_back(argv[i]);
    }

    config.lo2s_command_line = fmt::format("{}", fmt::join(lo2s_command_line, " "));
}

static void set_otf2_trace_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.trace_path = arguments.get("output-trace");
}

static void set_program_under_test_options(lo2s::Config& config,
                                           nitro::options::arguments& arguments)
{
    config.command = arguments.positionals();
    config.drop_root = arguments.given("drop-root");

    if (arguments.provided("as-user"))
    {
        config.drop_root = true;
        config.user = arguments.get("as-user");
    }
}

static void set_perf_general_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.mmap_pages = arguments.as<std::size_t>("mmap-pages");

    if (arguments.provided("perf-readout-interval"))
    {
        config.perf_read_interval =
            std::chrono::milliseconds(arguments.as<std::uint64_t>("perf-readout-interval"));
    }
    if (arguments.provided("cgroup"))
    {
        config.cgroup_fd = get_cgroup_mountpoint_fd(arguments.get("cgroup"));

        if (config.cgroup_fd == -1)
        {
            Log::fatal() << "Can not open cgroup directory for " << arguments.get("cgroup");
            std::exit(EXIT_FAILURE);
        }
    }
}

static void set_perf_instruction_sampling_options(lo2s::Config& config,
                                                  nitro::options::arguments& arguments)
{
    if (!arguments.provided("instruction-sampling"))
    {
        if (arguments.given("all-cpus-sampling"))
        {
            config.use_perf_sampling = true;
        }
        else if (arguments.given("all-cpus"))
        {
            config.use_perf_sampling = false;
        }
        else
        {
            config.use_perf_sampling = true;
        }
    }
    else
    {
        config.use_perf_sampling = arguments.given("instruction-sampling");
    }

    if (!arguments.provided("process-recording"))
    {
        if (arguments.given("all-cpus") || arguments.given("all-cpus-sampling"))
        {
            config.use_process_recording = true;
        }
        else
        {
            config.use_process_recording = false;
        }
    }
    else
    {
        config.use_process_recording = arguments.given("process-recording");
    }

    config.perf_sampling_period = arguments.as<std::uint64_t>("count");
    config.perf_sampling_event = arguments.get("event");

    config.exclude_kernel = !static_cast<bool>(arguments.given("kernel"));
    config.enable_callgraph = arguments.given("call-graph");
    config.disassemble = arguments.given("disassemble");

    std::string dwarf_mode = arguments.get("dwarf");
    if (dwarf_mode == "full")
    {
        config.dwarf = DwarfUsage::FULL;

        char* debuginfod_urls = getenv("DEBUGINFOD_URLS");
        if (debuginfod_urls == nullptr)
        {
            Log::warn()
                << "DEBUGINFOD_URLS not set! Will not be able to lookup debug info via debuginfod!";
        }
    }
    else if (dwarf_mode == "local")
    {
        config.dwarf = DwarfUsage::LOCAL;

        setenv("DEBUGINFOD_URLS", "", 1);
    }
    else if (dwarf_mode == "none")
    {
        config.dwarf = DwarfUsage::NONE;
    }
    else
    {
        std::cerr << "Unknown DWARF mode: " << dwarf_mode << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

static void set_python_sampling_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.use_python = arguments.given("python");
}

static void set_perf_tracepoint_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.tracepoint_events = arguments.get_all("tracepoint");
}

static void set_perf_io_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.use_block_io = arguments.given("block-io");
    config.use_posix_io = arguments.given("posix-io");
}

static void set_perf_syscall_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    if (arguments.provided("syscall"))
    {
        std::vector<std::string> requested_syscalls = arguments.get_all("syscall");
        config.use_syscalls = true;

        if (std::find(requested_syscalls.begin(), requested_syscalls.end(), "all") ==
            requested_syscalls.end())
        {
            config.syscall_filter = parse_syscall_names(requested_syscalls);
        }
    }
}

static void set_perf_metric_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.userspace_read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("userspace-readout-interval"));

    // Use time interval based metric recording as a default
    if (!arguments.provided("metric-leader"))
    {
        config.metric_use_frequency = true;
        config.metric_frequency = arguments.as<std::uint64_t>("metric-frequency");
    }
    else
    {
        config.metric_use_frequency = false;
        config.metric_count = arguments.as<std::uint64_t>("metric-count");
    }

    config.group_counters = arguments.get_all("metric-event");
    config.userspace_counters = arguments.get_all("userspace-metric-event");

    if (arguments.given("standard-metrics"))
    {
        for (const auto& mem_event : platform::get_mem_events())
        {
            config.group_counters.emplace_back(mem_event.name());
        }
        config.group_counters.emplace_back("instructions");
        config.group_counters.emplace_back("cpu-cycles");
    }

    for (const auto& event : config.userspace_counters)
    {
        auto it = std::find(config.group_counters.begin(), config.group_counters.end(), event);

        if (it != config.group_counters.end())
        {
            Log::warn()
                << event
                << " given as both userspace and grouped metric event only using it in userspace "
                   "measuring mode";
            config.group_counters.erase(it);
        }
    }

    if (!config.metric_leader.empty() || !config.group_counters.empty())
    {
        config.use_group_metrics = true;
    }
    if (!config.userspace_counters.empty())
    {
        config.use_userspace_metrics = true;
    }
    config.metric_leader = arguments.get("metric-leader");
}

static void set_perf_clock_options(lo2s::Config& config, nitro::options::arguments& arguments)
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
            config.use_pebs = true;
        }
        const auto& clock = lo2s::time::ClockProvider::get_clock_by_name(requested_clock_name);

        lo2s::Log::debug() << "Using clock \'" << clock.name << "\'.";
#ifndef USE_HW_BREAKPOINT_COMPAT
        config.clockid = clock.id;
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
    }
    catch (const lo2s::time::ClockProvider::InvalidClock& e)
    {
        lo2s::Log::fatal() << "Invalid clock requested: " << e.what();
        std::exit(EXIT_FAILURE);
    }
}

static void set_x86adapt_energy_options(lo2s::Config& config, nitro::options::arguments& arguments)
{

    config.use_x86_energy = arguments.given("x86-energy");
    config.x86_adapt_knobs = arguments.get_all("x86-adapt-knob");
}

static void set_sensor_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.use_sensors = arguments.given("sensors");
}

static void set_accelerator_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.nec_read_interval =
        std::chrono::microseconds(arguments.as<std::uint64_t>("nec-readout-interval"));

    config.nec_check_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("nec-check-interval"));

    for (const auto& accel : arguments.get_all("accel"))
    {
        if (accel == "nec")
        {
#ifdef HAVE_VEOSINFO
            config.use_nec = true;
#else
            std::cerr << "lo2s was built without support for NEC SX-Aurora sampling\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "nvidia")
        {
#ifdef HAVE_CUDA
            config.use_nvidia = true;
#else
            std::cerr << "lo2s was built without support for CUDA kernel recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "openmp")
        {
#ifdef HAVE_OPENMP
            config.use_openmp = true;
#else
            std::cerr << "lo2s was built without support for OpenMP recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "amd")
        {
#ifdef HAVE_HIP
            config.use_hip = true;
#else
            std::cerr << "lo2s was built without support for AMD HIP recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else
        {
            std::cerr << "Unknown Accelerator " << accel << "!";
            std::exit(EXIT_FAILURE);
        }
    }
}

static void set_ringbuf_options(lo2s::Config& config, nitro::options::arguments& arguments)
{
    config.socket_path = arguments.get("socket");
    config.injectionlib_path = arguments.get("ld-library-path");
    config.ringbuf_size = arguments.as<uint64_t>("ringbuf-size");

    config.ringbuf_read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("ringbuf-read-interval"));
}

static void check_print_options(nitro::options::parser& parser,
                                nitro::options::arguments& arguments)
{
    if (arguments.given("list-knobs"))
    {
#ifdef HAVE_X86_ADAPT

        std::map<std::string, std::string> node_knobs = metric::x86_adapt::x86_adapt_node_knobs();
        std::cout << io::make_argument_list("x86_adapt node knobs", node_knobs.begin(),
                                            node_knobs.end());

        std::map<std::string, std::string> cpu_knobs = metric::x86_adapt::x86_adapt_cpu_knobs();
        std::cout << io::make_argument_list("x86_adapt CPU knobs", cpu_knobs.begin(),
                                            cpu_knobs.end());
        std::exit(EXIT_SUCCESS);
#else
        std::cerr << "lo2s was built without support for x86_adapt; cannot read knobs.\n";
        std::exit(EXIT_FAILURE);
#endif
    }
    if (arguments.given("list-clockids"))
    {
        auto& clockids = time::ClockProvider::get_descriptions();
        std::cout << io::make_argument_list("available clockids", std::begin(clockids),
                                            std::end(clockids));
        std::exit(EXIT_SUCCESS);
    }

    if (arguments.given("list-events"))
    {
        print_availability(std::cout, "predefined events",
                           perf::EventResolver::instance().get_predefined_events());
        // TODO: find a better solution ?
        std::vector<perf::SysfsEventAttr> sys_events =
            perf::EventResolver::instance().get_pmu_events();
        std::vector<perf::EventAttr> events(sys_events.begin(), sys_events.end());
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

    if (arguments.given("list-tracepoints"))
    {
        std::vector<std::string> tracepoints =
            perf::EventResolver::instance().get_tracepoint_event_names();

        if (tracepoints.empty())
        {
            std::cout << "No tracepoints found!\n";
            std::cout << "Make sure that the tracefs is mounted and that you have read-execute "
                         "access to it.\n";
            std::cout << "\n";
            std::cout << "For more information, see the section TRACEPOINT in the man-page\n";
            std::exit(EXIT_FAILURE);
        }
        else
        {
            list_arguments_sorted(std::cout, "Kernel tracepoint events", tracepoints);
            std::exit(EXIT_SUCCESS);
        }
    }
    if (arguments.given("help"))
    {
        parser.usage();
        std::exit(EXIT_SUCCESS);
    }

    if (arguments.given("version"))
    {
        print_version(std::cout);
        std::exit(EXIT_SUCCESS);
    }
}

static void check_program_under_test_options(lo2s::Config& config)
{
    if (config.monitor_type == lo2s::MonitorType::PROCESS && config.process == Process::invalid() &&
        config.command.empty())
    {
        lo2s::Log::fatal() << "No process to monitor provided. "
                              "You need to pass either a COMMAND or a PID.";
        std::exit(EXIT_FAILURE);
    }

    if (config.drop_root && (std::getenv("SUDO_UID") == nullptr) && (config.user.empty()))
    {
        Log::error() << "-u was specified but no sudo was detected.";
        std::exit(EXIT_FAILURE);
    }

    if (config.monitor_type == lo2s::MonitorType::CPU_SET && config.use_posix_io)
    {
        Log::fatal() << "POSIX I/O recording can only be enabled in process monitoring mode.";
        std::exit(EXIT_FAILURE);
    }
}

static void check_perf_options(lo2s::Config& config)
{
    if (config.use_perf())
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

        if (config.use_any_tracepoint())
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

        if (perf::perf_event_paranoid() > 1 && !config.exclude_kernel)
        {
            std::cerr << "You requested kernel sampling, but kernel.perf_event_paranoid > 1, "
                         "retrying without kernel samples."
                      << std::endl;
            std::cerr << "To solve this warning you can do one of the following:" << std::endl;
            std::cerr << " * sysctl kernel.perf_event_paranoid=1" << std::endl;
            std::cerr << " * run lo2s as root" << std::endl;
            std::cerr << " * run with --no-kernel to disable kernel space sampling in "
                         "the first place,"
                      << std::endl;
            config.exclude_kernel = 1;
        }

        if (config.monitor_type == MonitorType::CPU_SET)
        {
            if (perf::perf_event_paranoid() > 0)
            {
                std::cerr << "You requested system-wide perf measurements, but "
                             "kernel.perf_event_paranoid > 0, "
                          << std::endl;
                std::cerr
                    << "To be able to do system-wide perf measurements in lo2s, do one of the "
                       "following:"
                    << std::endl;
                std::cerr << " * sysctl kernel.perf_event_paranoid=0" << std::endl;
                std::cerr << " * run lo2s as root" << std::endl;

                std::exit(1);
            }
        }

        if (config.use_perf_sampling &&
            !perf::EventResolver::instance().has_event(config.perf_sampling_event))
        {
            lo2s::Log::fatal() << "requested sampling event \'" << config.perf_sampling_event
                               << "\' is not available!";
            std::exit(EXIT_FAILURE); // hmm...
        }
        if (config.cgroup_fd != -1 && config.monitor_type == MonitorType::PROCESS)
        {
            Log::fatal() << "cgroup filtering can only be used in system-wide monitoring mode";
            std::exit(EXIT_FAILURE);
        }
    }
}

static void check_perf_metric_options(lo2s::Config& config)
{
    if (!config.metric_use_frequency && config.metric_leader.empty())
    {
        Log::fatal() << "--metric-count can only be used in conjunction with a --metric-leader";
        std::exit(EXIT_FAILURE);
    }

    if (config.metric_use_frequency && !config.metric_leader.empty())
    {
        Log::fatal() << "--metric-frequency can only be used with the default --metric-leader";
        std::exit(EXIT_FAILURE);
    }
    if (config.metric_leader.empty() && config.metric_frequency == 0)
    {
        Log::fatal()
            << "--metric-frequency should not be zero when using the default metric leader";
        std::exit(EXIT_FAILURE);
    }
    if (!config.metric_leader.empty() && config.metric_count == 0)
    {
        Log::fatal()
            << "--metric-count needs to be provided and not == 0 if a custom metric leader is used";
        std::exit(EXIT_FAILURE);
    }
}

static void check_optional_features(lo2s::Config& config)
{
#ifndef HAVE_X86_ADAPT
    if (!config.x86_adapt_knobs.empty())
    {
        Log::fatal() << "lo2s was built without support for x86_adapt; "
                        "cannot request x86_adapt knobs.\n";
        std::exit(EXIT_FAILURE);
    }
#endif

#ifndef HAVE_X86_ENERGY
    if (config.use_x86_energy)
    {
        lo2s::Log::fatal() << "lo2s was built without support for x86_energy; "
                              "cannot request x86_energy readings.\n";
        std::exit(EXIT_FAILURE);
    }
#endif

#ifndef HAVE_SENSORS
    if (config.use_sensors)
    {
        lo2s::Log::fatal() << "lo2s was built without support for libsensors; "
                              "cannot request sensor recording.\n";
        std::exit(EXIT_FAILURE);
    }
#endif
#ifndef HAVE_RADARE
    if (config.disassemble)
    {
        lo2s::Log::warn() << "Disassemble requested, but not supported by this installation.";
    }
    config.disassemble = false;
#endif

#ifndef HAVE_DEBUGINFOD
    if (config.dwarf == DwarfUsage::FULL)
    {
        Log::warn() << "No Debuginfod support available, downgrading to use only local files!";
        config.dwarf = DwarfUsage::LOCAL;

        // If we unset DEBUGINFOD_URLS, it will only use locally cached debug info files
        setenv("DEBUGINFOD_URLS", "", 1);
    }
#endif
}

void parse_program_options(int argc, const char** argv)
{
    std::stringstream description;
    description << "Lightweight Node-Level Performance Monitoring" << std::endl << std::endl;
    description << "  " << argv[0] << " [options] [-a | -A] COMMAND\n  " << argv[0]
                << " [options] [-a | -A] -- COMMAND [args to command...]\n  " << argv[0]
                << " [options] [-a | -A] -p PID\n";

    nitro::options::parser parser("lo2s", description.str());
    parser.accept_positionals();
    parser.positional_metavar("COMMAND");

    auto& general_options = parser.group("Options");
    auto& system_mode_options = parser.group("System-monitoring mode options");
    auto& sampling_options = parser.group("Sampling options");
    auto& perf_metric_options = parser.group("perf metric options");
    auto& kernel_tracepoint_options = parser.group("Kernel tracepoint options");
    auto& x86_adapt_options = parser.group("x86_adapt options");
    auto& x86_energy_options = parser.group("x86_energy options");
    auto& sensors_options = parser.group("sensors options");
    auto& io_options = parser.group("I/O recording options");
    auto& accel_options = parser.group("Accelerator options");

    lo2s::Config config;

    general_options.toggle("help", "Show this help message.").short_name("h");

    general_options.toggle("version", "Print version information.").short_name("V");

    general_options.toggle("quiet", "Suppress output.").short_name("q");
    general_options
        .toggle("verbose",
                "Verbose output (specify multiple times to get increasingly more verbose output).")
        .short_name("v");

    general_options.option("output-trace", "Output trace directory.")
        .default_value("lo2s_trace_{DATE}")
        .env("LO2S_OUTPUT_TRACE")
        .short_name("o");

    general_options.option("pid", "Attach to the process with the given PID.")
        .short_name("p")
        .metavar("PID")
        .optional();

    general_options.toggle("drop-root", "Drop root privileges before launching COMMAND.")
        .short_name("u");

    general_options.option("as-user", "Launch the COMMAND as the user USERNAME.")
        .short_name("U")
        .metavar("USERNAME")
        .optional();

    general_options.option("mmap-pages", "Number of pages to be used by internal buffers.")
        .short_name("m")
        .default_value("16")
        .metavar("PAGES");

    general_options
        .option("readout-interval", "Time in milliseconds between readouts of interval based "
                                    "monitors, i.e. x86_adapt, x86_energy.")
        .short_name("i")
        .default_value("100")
        .metavar("MSEC");

    general_options
        .option("perf-readout-interval",
                "Time in milliseconds between readouts of perf based monitors, i.e. sampling, "
                "metrics, tracepoints. If not provided, interval based readouts are disabled.")
        .short_name("I")
        .optional()
        .metavar("MSEC");

    general_options.option("clockid", "Reference clock used as timestamp source.")
        .short_name("k")
        .default_value("monotonic-raw")
        .metavar("CLOCKID");

    general_options.toggle("list-clockids", "List all available clockids.");

    general_options.toggle("list-events", "List available metric and sampling events.");
    general_options.toggle("list-tracepoints", "List available kernel tracepoint events.");

    general_options.toggle("list-knobs", "List available x86_adapt CPU knobs.");

    general_options
        .option("dwarf",
                "Set DWARF resolve mode. 'none' disables DWARF, 'local' uses only locally cached "
                "debuginfo files, 'full' uses debuginfod to download debug information on demand")
        .default_value("local")
        .metavar("DWARFMODE");

    system_mode_options
        .toggle("process-recording", "Record process activity. In system monitoring: ")
        .allow_reverse()
        .default_value(true);

    general_options.option("socket", "Path to socket used for ringbuffer communication.")
        .default_value("/tmp/lo2s.socket")
        .metavar("PATH");

    system_mode_options
        .toggle("all-cpus", "Start in system-monitoring mode for all CPUs. "
                            "Monitor as long as COMMAND is running or until PID exits.")
        .short_name("a");

    system_mode_options
        .toggle("all-cpus-sampling", "System-monitoring mode with instruction sampling. "
                                     "Shorthand for \"-a --instruction-sampling\".")
        .short_name("A");

    system_mode_options
        .option("cgroup",
                "Only record perf events for the given cgroup. Can only be used in system-mode")
        .metavar("NAME")
        .optional();

    sampling_options
        .toggle("instruction-sampling", "Enable instruction sampling. In system monitoring: "
                                        "(default: disabled). In process monitoring:")
        .default_value(true)
        .allow_reverse();

    sampling_options.option("event", "Interrupt source event for sampling.")
        .short_name("e")
        .metavar("EVENT")
        .default_value(Topology::instance().hypervised() ? "cpu-clock" : "instructions");

    sampling_options.option("count", "Sampling period (in number of events specified by -e).")
        .short_name("c")
        .default_value("11010113")
        .metavar("N");

    sampling_options.toggle("call-graph", "Record call stack of instruction samples.")
        .short_name("g");

    sampling_options.toggle("no-ip",
                            "Do not record instruction pointers [NOT CURRENTLY SUPPORTED]");

    sampling_options
        .toggle("disassemble", "Enable augmentation of samples with instructions.")
#ifdef HAVE_RADARE
        .default_value(true)
#endif
        .allow_reverse();

    sampling_options.toggle("kernel", "Include events happening in kernel space.")
        .allow_reverse()
        .default_value(true);

    sampling_options.toggle("python", "Control Python perf recording support")
        .allow_reverse()
        .default_value(true);

    kernel_tracepoint_options
        .multi_option("tracepoint",
                      "Enable global recording of a raw tracepoint event (usually requires root).")
        .short_name("t")
        .optional()
        .metavar("TRACEPOINT");

    perf_metric_options.multi_option("metric-event", "Record metrics for this perf event.")
        .short_name("E")
        .optional()
        .metavar("EVENT");

    perf_metric_options
        .multi_option("userspace-metric-event",
                      "Record metrics for this perf event. (Slower, but might "
                      "work for events for which --metric-event doesn't work)")
        .optional()
        .metavar("EVENT");

    perf_metric_options
        .option("userspace-readout-interval",
                "Readout interval for metrics specified by --userspace-metric-event")
        .metavar("MSEC")
        .default_value("100");

    perf_metric_options.toggle("standard-metrics", "Record a set of default metrics.");

    perf_metric_options
        .option("metric-leader", "The leading metric event. This event is used as an interval "
                                 "giver for the other metric events.")
        .optional()
        .default_value("")
        .metavar("EVENT");

    perf_metric_options
        .option("metric-count", "Number of metric leader events to elapse before reading metric "
                                "buffer. Has to be used in conjunction with --metric-leader")
        .metavar("N")
        .optional();

    perf_metric_options
        .option("metric-frequency",
                "Number of metric buffer reads per second. Can not be used with --metric-leader")
        .metavar("HZ")
        .default_value("10");

    perf_metric_options
        .multi_option("syscall",
                      "Record syscall events for given syscall. \"all\" to record all syscalls")
        .short_name("s")
        .metavar("SYSCALL")
        .optional();

    x86_adapt_options
        .multi_option("x86-adapt-knob",
                      "Record the given x86_adapt knob. Append #accumulated_last for semantics.")
        .short_name("x")
        .optional()
        .metavar("KNOB");

    x86_energy_options.toggle("x86-energy", "Add x86_energy recordings.").short_name("X");

    sensors_options.toggle("sensors", "Record sensors using libsensors.").short_name("S");

    io_options.toggle("block-io",
                      "Enable recording of block I/O events (requires access to tracefs)");

    std::vector<std::string> accelerators;

#ifdef HAVE_CUDA
    accelerators.push_back("nvidia");
#endif
#ifdef HAVE_VEOSINFO
    accelerators.push_back("nec");
#endif
#ifdef HAVE_OPENMP
    accelerators.push_back("openmp");
#endif

    accel_options
        .multi_option(
            "accel",
            fmt::format("Accelerator to record execution events for. Available accelerators: {}",
                        fmt::join(accelerators, ", ")))
        .metavar("ACCEL")
        .optional();

    accel_options.option("nec-readout-interval", "Accelerator sampling interval")
        .optional()
        .metavar("USEC")
        .default_value("1");
    accel_options.option("nec-check-interval", "The interval between checks for new VE processes")
        .optional()
        .metavar("MSEC")
        .default_value("100");
    accel_options.option("ringbuf-read-interval", "interval for readings of the ring buffer")
        .optional()
        .metavar("MSEC")
        .default_value("100");
    accel_options.option("ld-library-path", "path to the lo2s injection library")
        .optional()
        .metavar("PATH")
        .default_value(LO2S_INJECTIONLIB_PATH);

    accel_options.option("ringbuf-size", "Size of the injection library ring-buffer")
        .optional()
        .metavar("PAGES")
        .default_value("16");

    io_options.toggle("posix-io",
                      "Enable recording of POSIX I/o events (requires access to tracefs)");

    nitro::options::arguments arguments;
    try
    {
        arguments = parser.parse(argc, argv);
    }
    catch (const nitro::options::parsing_error& e)
    {
        std::cerr << e.what() << '\n';
        parser.usage();
        std::exit(EXIT_FAILURE);
    }
    set_verbosity(config, arguments);
    // Check all the options that just print something
    check_print_options(parser, arguments);

    // First step: Set all the values in the config
    set_general_options(config, arguments, argc, argv);

    set_otf2_trace_options(config, arguments);

    set_program_under_test_options(config, arguments);

    set_perf_general_options(config, arguments);

    set_perf_instruction_sampling_options(config, arguments);

    set_python_sampling_options(config, arguments);

    set_perf_tracepoint_options(config, arguments);

    set_perf_io_options(config, arguments);

    set_perf_syscall_options(config, arguments);

    set_perf_metric_options(config, arguments);

    set_perf_clock_options(config, arguments);

    set_x86adapt_energy_options(config, arguments);

    set_accelerator_options(config, arguments);

    set_ringbuf_options(config, arguments);

    set_sensor_options(config, arguments);

    // Check correctness of given config

    // Check presence of optional compile-time features
    check_optional_features(config);

    check_program_under_test_options(config);
    check_perf_options(config);
    check_perf_metric_options(config);

    instance = std::move(config);
}
} // namespace lo2s
