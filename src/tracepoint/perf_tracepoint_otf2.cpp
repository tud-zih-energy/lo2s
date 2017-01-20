#include "../otf2_trace.hpp"

#include <nitro/lang/enumerate.hpp>

#include "perf_tracepoint_otf2.hpp"

namespace lo2s
{
perf_tracepoint_otf2::perf_tracepoint_otf2(int cpu, const event_format& event,
                                           const monitor_config& config, otf2_trace& trace,
                                           const otf2::definition::metric_class& metric_class,
                                           const perf_time_converter& time_converter)
: perf_tracepoint_reader(cpu, event.id(), config.mmap_pages), event_(event),
  writer_(trace.metric_writer((boost::format("raw metrics for CPU %d") % cpu).str())),
  metric_instance_(
      trace.metric_instance(metric_class, writer_.location(), trace.system_tree_cpu_node(cpu))),
  time_converter_(time_converter)
{
    counter_values_.resize(metric_class.size());
    for (std::size_t i = 0; i < metric_class.size(); i++)
    {
        counter_values_[i].metric = metric_class[i];
    }
}

bool perf_tracepoint_otf2::handle(const perf_tracepoint_reader::record_sample_type* sample)
{
    auto tp = time_converter_(sample->time);

    for (const auto& index_field : nitro::lang::enumerate(event_.fields()))
    {
        const auto& index = index_field.index();
        const auto& field = index_field.value();
        counter_values_[index].set(sample->raw_data.get(field));
    }
    writer_.write(otf2::event::metric(tp, metric_instance_, counter_values_));
    return false;
}
}
