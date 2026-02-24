#include <lo2s/metric/x86_adapt/monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/enumerate.hpp>
#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/definition/metric_class.hpp>
#include <x86_adapt_cxx/configuration_item.hpp>
#include <x86_adapt_cxx/device.hpp>

#include <string>
#include <utility>
#include <vector>

#include <cassert>

namespace lo2s::metric::x86_adapt
{

Monitor::Monitor(::x86_adapt::device device,
                 const std::vector<::x86_adapt::configuration_item>& configuration_items,
                 trace::Trace& trace, const otf2::definition::metric_class& metric_class)
: PollMonitor(trace, std::to_string(device.id()), config().read_interval),
  device_(std::move(device)), otf2_writer_(trace.create_metric_writer(name())),
  configuration_items_(configuration_items),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(),
                                         trace.system_tree_cpu_node(Cpu(device_.id())))),
  event_(otf2::chrono::genesis(), metric_instance_)
{
    assert(device_.type() == X86_ADAPT_CPU);
}

void Monitor::initialize_thread()
{
    try_pin_to_scope(ExecutionScope(Cpu(device_.id())));
}

void Monitor::monitor([[maybe_unused]] int fd)
{
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
} // namespace lo2s::metric::x86_adapt
