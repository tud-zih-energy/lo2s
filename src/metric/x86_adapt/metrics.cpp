#include <lo2s/metric/x86_adapt/metrics.hpp>

#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{

Metrics::Metrics(trace::Trace& trace, const std::vector<std::string>& item_names)
{
    auto mc = trace.metric_class();
    auto node_mc = trace.metric_class();

    std::vector<::x86_adapt::configuration_item> cpu_configuration_items;
    std::vector<::x86_adapt::configuration_item> node_configuration_items;

    for (auto item_name : item_names)
    {
        auto mode = guess_mode(item_name);

        bool found = false;

        for (const auto& node_item : x86_adapt_.node_configuration_items())
        {
            if (node_item.name() == item_name)
            {
                Log::info() << "Adding x86_adapt node knob: " << item_name;

                node_configuration_items.emplace_back(node_item);
                node_mc.add_member(trace.metric_member(item_name, node_item.description(), mode,
                                                       otf2::common::type::int64, "#"));
                found = true;
                break;
            }
        }

        if (found)
        {
            continue;
        }

        for (const auto& cpu_item : x86_adapt_.cpu_configuration_items())
        {
            if (cpu_item.name() == item_name)
            {
                Log::info() << "Adding x86_adapt CPU knob: " << item_name;

                cpu_configuration_items.emplace_back(cpu_item);
                mc.add_member(trace.metric_member(item_name, cpu_item.description(), mode,
                                                  otf2::common::type::int64, "#"));
                break;
            }
        }
    }

    for (const auto& cpu : Topology::instance().cpus())
    {
        recorders_.emplace_back(
            std::make_unique<Monitor>(x86_adapt_.cpu(cpu.id), cpu_configuration_items, trace, mc));
    }

    for (const auto& package : Topology::instance().packages())
    {
        node_recorders_.emplace_back(std::make_unique<NodeMonitor>(
            x86_adapt_.node(package.id), node_configuration_items, trace, node_mc));
    }
}

void Metrics::start()
{
    for (auto& recorder : recorders_)
    {
        recorder->start();
    }

    for (auto& recorder : node_recorders_)
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

    for (auto& recorder : node_recorders_)
    {
        recorder->stop();
    }
}
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
