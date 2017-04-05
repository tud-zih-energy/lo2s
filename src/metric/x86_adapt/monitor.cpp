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

Monitor::Monitor(::x86_adapt::device device, std::chrono::nanoseconds sampling_interval,
                 const std::vector<::x86_adapt::configuration_item>& configuration_items,
                 trace::Trace& trace, const otf2::definition::metric_class& metric_class)
: IntervalMonitor(trace, std::to_string(device.id()), sampling_interval),
  device_(std::move(device)), otf2_writer_(trace.metric_writer(name())),
  configuration_items_(configuration_items),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(),
                                         trace.system_tree_cpu_node(device_.id())))
// (WOW this is (almost) better than LISP)
{
    assert(device_.type() == X86_ADAPT_CPU);

    metric_values_.resize(configuration_items.size());
    for (std::size_t i = 0; i < configuration_items_.size(); i++)
    {
        metric_values_[i].metric = metric_instance_.metric_class()[i];
    }
}

void Monitor::initialize_thread()
{
    try_pin_to_cpu(device_.id());
}

void Monitor::monitor()
{
    auto read_time = time::now();
    for (const auto& index_ci : nitro::lang::enumerate(configuration_items_))
    {
        metric_values_[index_ci.index()].set(device_(index_ci.value()));
    }

    // TODO optimize! (avoid copy, avoid shared pointers...)
    otf2_writer_.write(otf2::event::metric(read_time, metric_instance_, metric_values_));
}
}
}
}
