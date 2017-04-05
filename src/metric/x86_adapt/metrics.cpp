#include <lo2s/metric/x86_adapt/metrics.hpp>

#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{

Metrics::Metrics(trace::Trace& trace, std::chrono::nanoseconds sampling_interval,
                 const std::vector<std::string>& cpu_item_names)
{
    auto mc = trace.metric_class();

    std::vector<::x86_adapt::configuration_item> configuration_items;
    for (auto cpu_item_name : cpu_item_names)
    {
        auto mode = guess_mode(cpu_item_name);

        configuration_items.emplace_back(
            x86_adapt_.cpu_configuration_items().lookup(cpu_item_name));
        mc.add_member(trace.metric_member(cpu_item_name, "x86_adapt metric", mode,
                                          otf2::common::type::int64, "#"));
    }
    for (const auto& cpu : Topology::instance().cpus())
    {
        recorders_.emplace_back(std::make_unique<Monitor>(
            x86_adapt_.cpu(cpu.id), sampling_interval, configuration_items, trace, mc));
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
}
}
}
