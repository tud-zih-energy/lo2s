#include <lo2s/config/general_config.hpp>

#include <lo2s/config/monitor_type.hpp>
#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/version.hpp>

#include <nitro/log/severity.hpp>
#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <ostream>
#include <vector>

#include <cstdlib>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <sys/types.h>

namespace lo2s
{

namespace
{

void print_version(std::ostream& os)
{
    // clang-format off
    os << "lo2s " << lo2s::version() << "\n"
       << "Copyright (C) " LO2S_COPYRIGHT_YEAR " Technische Universitaet Dresden, Germany\n"
       << "This is free software; see the source for copying conditions.  There is NO\n"
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    // clang-format on
}
} // namespace

GeneralConfig::GeneralConfig(nitro::options::arguments& arguments, int argc, const char** argv)
{
    quiet = arguments.given("quiet");
    if (arguments.given("quiet") && arguments.given("verbose"))
    {
        Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        quiet = false;
    }
    if (arguments.given("all-cpus") || arguments.given("all-cpus-sampling"))
    {
        monitor_type = MonitorType::CPU_SET;
    }
    else
    {
        monitor_type = MonitorType::PROCESS;
    }
    process = arguments.provided("pid") ? Process(arguments.as<pid_t>("pid")) : Process::invalid();

    lo2s_command_line = "";

    // The entire command line of lo2s, to be written to the trace
    std::vector<std::string> command_line_vec(argc);
    for (int i = 0; i < argc; i++)
    {
        command_line_vec.emplace_back(argv[i]);
    }

    lo2s_command_line = fmt::format("{}", fmt::join(command_line_vec, " "));
}

void GeneralConfig::set_verbosity(nitro::options::arguments& arguments)
{
    if (arguments.given("quiet"))
    {
        logging::set_min_severity_level(nitro::log::severity_level::error);
    }
    else
    {
        using sl = nitro::log::severity_level;
        switch (arguments.given("verbose"))
        {
        case 0:
            logging::set_min_severity_level(sl::warn);
            break;
        case 1:
            Log::info() << "Enabling log-level 'info'";
            lo2s::logging::set_min_severity_level(sl::info);
            break;
        case 2:
            Log::info() << "Enabling log-level 'debug'";
            lo2s::logging::set_min_severity_level(sl::debug);
            break;
        case 3:
        default:
            Log::info() << "Enabling log-level 'trace'";
            logging::set_min_severity_level(sl::trace);
            break;
        }
    }
}

void GeneralConfig::add_parser(nitro::options::parser& parser)

{
    auto& general_options = parser.group("Options");
    general_options
        .toggle("all-cpus", "Start in system-monitoring mode for all CPUs. "
                            "Monitor as long as COMMAND is running or until PID exits.")
        .short_name("a");

    general_options
        .toggle("all-cpus-sampling", "System-monitoring mode with instruction sampling. "
                                     "Shorthand for \"-a --instruction-sampling\".")
        .short_name("A");

    general_options.option("pid", "Attach to the process with the given PID.")
        .short_name("p")
        .metavar("PID")
        .optional();
    general_options.toggle("help", "Show this help message.").short_name("h");

    general_options.toggle("version", "Print version information.").short_name("V");
    general_options.toggle("dump-config", "Dump config to JSON on stdout");

    general_options.toggle("quiet", "Suppress output.").short_name("q");
    general_options
        .toggle("verbose",
                "Verbose output (specify multiple times to get increasingly more verbose output).")
        .short_name("v");
}

void GeneralConfig::parse_print_options(nitro::options::arguments& arguments)
{
    if (arguments.given("version"))
    {
        print_version(std::cout);
        std::exit(EXIT_SUCCESS);
    }
}

void to_json(nlohmann::json& j, const GeneralConfig& config)
{
    j = nlohmann::json{ { "quiet", config.quiet },
                        { "monitor_type", config.monitor_type },
                        { "process", config.process.as_int() },
                        { "lo2s_command_line", config.lo2s_command_line } };
};
} // namespace lo2s
