#include <lo2s/metric/x86_adapt/node_monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/trace/trace.hpp>

#include <string>

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{

NodeMonitor::NodeMonitor(::x86_adapt::device device, std::chrono::nanoseconds sampling_interval,
                         const std::vector<::x86_adapt::configuration_item>& configuration_items,
                         trace::Trace& trace, const otf2::definition::metric_class& metric_class)
: IntervalMonitor(trace, std::to_string(device.id()), sampling_interval),
  device_(std::move(device)), otf2_writer_(trace.metric_writer(name())),
  configuration_items_(configuration_items),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(),
                                         trace.system_tree_package_node(device_.id())))
// (WOW this is (almost) better than LISP)
{
    assert(device_.type() == X86_ADAPT_DIE);

    metric_values_.resize(configuration_items.size());
    for (std::size_t i = 0; i < configuration_items_.size(); i++)
    {
        metric_values_[i].metric = metric_instance_.metric_class()[i];
    }
}

void NodeMonitor::initialize_thread()
{
    auto package = lo2s::Topology::instance().packages().at(device_.id());
    try_pin_to_cpu(*(package.cpu_ids.begin()));
}

void NodeMonitor::monitor()
{
    auto read_time = time::now();
    for (const auto& index_ci : nitro::lang::enumerate(configuration_items_))
    {
        metric_values_[index_ci.index()].set(device_(index_ci.value()));
    }

    // TODO optimize! (avoid copy, avoid shared pointers...)
    otf2_writer_.write(otf2::event::metric(read_time, metric_instance_, metric_values_));
}
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
