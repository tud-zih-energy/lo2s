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

NodeMonitor::NodeMonitor(::x86_adapt::device device,
                         const std::vector<::x86_adapt::configuration_item>& configuration_items,
                         trace::Trace& trace, const otf2::definition::metric_class& metric_class)
: PollMonitor(trace, std::to_string(device.id()), config().read_interval),
  device_(std::move(device)), otf2_writer_(trace.create_metric_writer(name())),
  configuration_items_(configuration_items),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(),
                                         trace.system_tree_package_node(device_.id()))),
  event_(otf2::chrono::genesis(), metric_instance_)
{
    assert(device_.type() == X86_ADAPT_DIE);
}

void NodeMonitor::initialize_thread()
{
    auto package = lo2s::Topology::instance().packages().at(device_.id());
    try_pin_to_scope(ExecutionScope::cpu(*(package.cpu_ids.begin())));
}

void NodeMonitor::monitor([[maybe_unused]] int fd)
{
    event_.timestamp(time::now());
    for (const auto& index_ci : nitro::lang::enumerate(configuration_items_))
    {
        auto i = index_ci.index();
        auto configuration_item = index_ci.value();

        event_.raw_values()[i] = device_(configuration_item);
    }

    otf2_writer_.write(event_);
}
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
