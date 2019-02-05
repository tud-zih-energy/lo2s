#include <lo2s/metric/x86_adapt/monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <string>

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{

Monitor::Monitor(::x86_adapt::device device,
                 const std::vector<::x86_adapt::configuration_item>& configuration_items,
                 trace::Trace& trace, const otf2::definition::metric_class& metric_class)
: PollMonitor(trace, std::to_string(device.id())), device_(std::move(device)),
  otf2_writer_(trace.named_metric_writer(name())), configuration_items_(configuration_items),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(),
                                         trace.system_tree_cpu_node(device_.id()))),
  event_(otf2::chrono::genesis(), metric_instance_)
{
    assert(device_.type() == X86_ADAPT_CPU);
}

void Monitor::initialize_thread()
{
    try_pin_to_cpu(device_.id());
}

void Monitor::monitor(int fd)
{
    void(fd);
    // update timestamp
    event_.timestamp(time::now());

    // update event values from configuration items
    for (const auto& index_ci : nitro::lang::enumerate(configuration_items_))
    {
        auto i = index_ci.index();
        auto configuration_item = index_ci.value();

        event_.raw_values()[i] = device_(configuration_item);
    }

    // write event to archive
    otf2_writer_.write(event_);
}
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
