#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/recorder.hpp>

#include <lo2s/trace/trace.hpp>

#include <nitro/lang/enumerate.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

Writer::Writer(int cpu, const EventFormat& event, const MonitorConfig& config,
               trace::Trace& trace_, const otf2::definition::metric_class& metric_class,
               const time::Converter& time_converter)
: Reader(cpu, event.id(), config.mmap_pages), event_(event),
  writer_(trace_.metric_writer((boost::format("tracepoint metrics for CPU %d") % cpu).str())),
  metric_instance_(
      trace_.metric_instance(metric_class, writer_.location(), trace_.system_tree_cpu_node(cpu))),
  time_converter_(time_converter)
{
    counter_values_.resize(metric_class.size());
    for (std::size_t i = 0; i < metric_class.size(); i++)
    {
        counter_values_[i].metric = metric_class[i];
    }
}

bool Writer::handle(const Reader::RecordSampleType* sample)
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
}
}
