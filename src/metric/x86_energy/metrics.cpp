#include <lo2s/metric/x86_energy/metrics.hpp>

#include <lo2s/log.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/trace/trace.hpp>

#include <sstream>
#include <string>

namespace xe = x86_energy;

namespace lo2s
{
namespace metric
{
namespace x86_energy
{

Metrics::Metrics(trace::Trace& trace, std::chrono::nanoseconds sampling_interval)
{
    xe::Mechanism mechanism;

    Log::debug() << "Using x86_energy mechanism: " << mechanism.name();

    std::unique_ptr<xe::AccessSource> active_source;

    auto sources = mechanism.available_sources();

    for (auto& source : sources)
    {
        try
        {
            source.init();

            active_source = std::make_unique<xe::AccessSource>(std::move(source));
            break;
        }
        catch (std::exception& e)
        {
            Log::info() << "Failed to initialize access source: " << source.name()
                        << " error was: " << e.what();
        }
    }

    if (!active_source)
    {
        Log::error()
            << "Failed to initialize any available source. x86_energy values won't be available.";
        throw std::runtime_error("Failed to initialize x86_energy access source.");
    }

    // if we get here, we have at least one access source to collect metric data from
    Log::info() << "Using x86_energy access source: " << active_source->name();

    // okay, we have
    for (int i = 0; i < static_cast<int>(xe::Counter::SIZE); i++)
    {
        auto counter = static_cast<xe::Counter>(i);
        auto granularity = mechanism.granularity(counter);

        if (granularity == xe::Granularity::SIZE)
        {
            Log::debug() << "Counter is not available: " << counter << " (Skipping)";

            continue;
        }

        std::stringstream str;
        str << mechanism.name() << " " << counter;

        std::string metric_name = str.str();

        auto mc = trace.metric_class();
        mc.add_member(trace.metric_member(metric_name, metric_name,
                                          otf2::common::metric_mode::accumulated_start,
                                          otf2::common::type::int64, "J"));

        // if we get here, we need to setup a source counter, a monitor thread and so on

        for (auto index = 0; index < architecture_.size(granularity); index++)
        {
            int cpuid = -1;
            int packageid = -1;
            int coreid = -1;

            for (const auto& cpu : lo2s::Topology::instance().cpus())
            {
                auto node = architecture_.get_arch_for_cpu(granularity, cpu.id);
                if (node.id() == index)
                {
                    cpuid = cpu.id;
                    packageid = cpu.package_id;
                    coreid = cpu.core_id;
                    break;
                }
            }

            if (cpuid == -1)
            {
                Log::warn() << "Cannot determine a cpu to pin the measurement thread for the "
                            << counter << " counter " << index << " Using cpu 0 instead.";
                cpuid = 0;
            }

            otf2::definition::system_tree_node stn;

            switch (granularity)
            {
            case xe::Granularity::SYSTEM:
                stn = trace.system_tree_root_node();
                break;
            case xe::Granularity::SOCKET:
            case xe::Granularity::DIE:
            case xe::Granularity::MODULE:
                stn = trace.system_tree_package_node(packageid);
                break;
            case xe::Granularity::CORE:
                stn = trace.system_tree_core_node(coreid, packageid);
                break;
            case xe::Granularity::THREAD:
                stn = trace.system_tree_cpu_node(cpuid);
                break;
            default:
                Log::warn() << "Cannot determine a suitable system tree node for the " << counter
                            << " counter. (Falling back to root node)";
                stn = trace.system_tree_root_node();
            }

            recorders_.emplace_back(std::make_unique<Monitor>(
                active_source->get(counter, index), cpuid, sampling_interval, trace, mc, stn));
        }
    }
}

void Metrics::start()
{
    for (auto& recorder : recorders_)
    {
        recorder->start();
    }
}

void Metrics::stop()
{
    for (auto& recorder : recorders_)
    {
        recorder->stop();
    }
}
} // namespace x86_energy
} // namespace metric
} // namespace lo2s
