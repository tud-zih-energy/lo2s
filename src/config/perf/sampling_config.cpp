#include <lo2s/config/perf/sampling_config.hpp>

#include <lo2s/log.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/topology.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <ostream>

#include <cstdint>
#include <cstdlib>

#include <unistd.h>

namespace lo2s::perf
{

SamplingConfig::SamplingConfig(nitro::options::arguments& arguments)
{
    if (!arguments.provided("instruction-sampling"))
    {
        enabled = arguments.given("all-cpus-sampling") || !arguments.given("all-cpus");
    }
    else
    {
        enabled = arguments.given("instruction-sampling");
    }

    if (!arguments.provided("process-recording"))
    {
        // TODO: This is ugly, as all-cpus is defined somewhere else
        process_recording = arguments.given("all-cpus") || arguments.given("all-cpus-sampling");
    }
    else
    {
        process_recording = arguments.given("process-recording");
    }
    exclude_kernel = !static_cast<bool>(arguments.given("kernel"));
    enable_callgraph = arguments.given("call-graph");
    disassemble = arguments.given("disassemble");
    period = arguments.as<std::uint64_t>("count");
    event = arguments.get("event");
}

void SamplingConfig::add_parser(nitro::options::parser& parser)
{
    auto& sampling_options = parser.group("perf sampling options");
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
    sampling_options.toggle("process-recording", "Record process activity. In system monitoring: ")
        .allow_reverse()
        .default_value(true);
}

void SamplingConfig::check()
{
#ifndef HAVE_RADARE
    if (disassemble)
    {
        lo2s::Log::warn() << "Disassemble requested, but not supported by this installation.";
    }
    disassemble = false;
#endif
    if (getuid() != 0 && perf::perf_event_paranoid() > 1 && exclude_kernel)
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
        exclude_kernel = true;
    }
    if (enabled && !perf::EventResolver::instance().has_event(event))
    {
        lo2s::Log::fatal() << "requested sampling event \'" << event << "\' is not available!";
        std::exit(EXIT_FAILURE); // hmm...
    }
}

void to_json(nlohmann::json& j, const perf::SamplingConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled },
                         { "process_recording", config.process_recording },
                         { "period", config.period },
                         { "event", config.event },
                         { "exclude_kernel", config.exclude_kernel },
                         { "enable_callgraph", config.enable_callgraph },
                         { "disassemble", config.disassemble },
                         { "use_pebs", config.use_pebs } });
}
} // namespace lo2s::perf
