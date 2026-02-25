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
#include <lo2s/config/accelerator_config.hpp>
#include <lo2s/config/dwarf_config.hpp>
#include <lo2s/config/general_config.hpp>
#include <lo2s/config/monitor_type.hpp>
#include <lo2s/config/otf2_config.hpp>
#include <lo2s/config/perf_config.hpp>
#include <lo2s/config/put_config.hpp>
#include <lo2s/config/rb_config.hpp>
#include <lo2s/config/sensors_config.hpp>
#include <lo2s/config/x86_adapt_config.hpp>
#include <lo2s/config/x86_energy_config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/types/process.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/exception.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#ifdef HAVE_LIBPFM
#endif
#include <lo2s/perf/util.hpp>

#ifdef HAVE_X86_ADAPT
#endif

#include <nitro/log/severity.hpp>
#include <nitro/options/exception.hpp>
#include <nitro/options/parser.hpp>

#include <cstdlib>

#include <fmt/core.h>
#include <fmt/format.h>

extern "C"
{
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{

namespace
{
std::optional<Config> instance;
}

const Config& config()
{
    if (!instance)
    {
        Log::error()
            << "Trying to access Config singleton, but config parsing has not finished yet!";
        Log::error() << "This is an implementation error, please report it";
        std::exit(EXIT_FAILURE);
    }
    return *instance;
}

/* Some parts of lo2s want to read config() before we are finished parsing it,
 * e.g. the event listing code. In this case supply it with a sane default config
 */

void parse_program_options(int argc, const char** argv)
{
    std::stringstream description;
    description << "Lightweight Node-Level Performance Monitoring" << std::endl << std::endl;
    description << "  " << argv[0] << " [options] [-a | -A] COMMAND\n  " << argv[0]
                << " [options] [-a | -A] -- COMMAND [args to command...]\n  " << argv[0]
                << " [options] [-a | -A] -p PID\n";

    nitro::options::parser parser("lo2s", description.str());

    Config::add_parser(parser);

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

    Config::set_verbosity(arguments);

    if (arguments.given("help"))
    {
        parser.usage();
        std::exit(EXIT_SUCCESS);
    }

    Config::parse_print_options(arguments);

    Config config(arguments, argc, argv);

    config.check();

    if (arguments.given("dump-config"))
    {
        nlohmann::json const j = config;
        std::cout << j.dump(4);
        std::exit(EXIT_SUCCESS);
    }
    instance = std::move(config);
}

void Config::add_parser(nitro::options::parser& parser)
{
    GeneralConfig::add_parser(parser);

    Otf2Config::add_parser(parser);
    ProgramUnderTestConfig::add_parser(parser);
    perf::Config::add_parser(parser);
    DwarfConfig::add_parser(parser);
    X86EnergyConfig::add_parser(parser);
    X86AdaptConfig::add_parser(parser);
    SensorsConfig::add_parser(parser);
    AcceleratorConfig::add_parser(parser);
    RingbufConfig::add_parser(parser);
}

void Config::check() const
{
    if (perf.posix_io.enabled)
    {
        if (getuid() != 0)
        {
            std::cerr << "POSIX I/O recording makes use of BPF" << std::endl;
            std::cerr << "BPF is currently only available to root" << std::endl;
            std::cerr << "please re-run lo2s as root." << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    if (perf.cgroup_fd != -1 && general.monitor_type == MonitorType::PROCESS)
    {
        Log::fatal() << "cgroup filtering can only be used in system-wide monitoring mode";
        std::exit(EXIT_FAILURE);
    }

    if (perf.any_perf())
    {
        if (general.monitor_type == MonitorType::CPU_SET)
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
    }
    if (general.monitor_type == lo2s::MonitorType::PROCESS &&
        general.process == Process::invalid() && put.command.empty())
    {
        lo2s::Log::fatal() << "No process to monitor provided. "
                              "You need to pass either a COMMAND or a PID.";
        std::exit(EXIT_FAILURE);
    }

    if (put.as_user.drop_root && (std::getenv("SUDO_UID") == nullptr) && (put.as_user.user.empty()))
    {
        Log::error() << "-u was specified but no sudo was detected.";
        std::exit(EXIT_FAILURE);
    }

    if (general.monitor_type == lo2s::MonitorType::CPU_SET && perf.posix_io.enabled)
    {
        Log::fatal() << "POSIX I/O recording can only be enabled in process monitoring mode.";
        std::exit(EXIT_FAILURE);
    }
}

void to_json(nlohmann::json& j, const Config& config)
{
    j = nlohmann::json({ { "general", config.general },
                         { "otf2", config.otf2 },
                         { "program_under_test", config.put },
                         { "perf", config.perf },
                         { "dwarf", config.dwarf },
                         { "x86_energy", config.x86_energy },
                         { "x86_adapt", config.x86_adapt },
                         { "sensors", config.sensors },
                         { "accel", config.accel },
                         { "ringbuf", config.rb } });
}
} // namespace lo2s

// namespace lo2s
