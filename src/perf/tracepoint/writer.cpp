#include <lo2s/config.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/tracepoint/metric_monitor.hpp>

#include <lo2s/trace/trace.hpp>

#include <nitro/lang/enumerate.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

Writer::Writer(int cpu, const EventFormat& event, trace::Trace& trace_,
               const otf2::definition::metric_class& metric_class)
: Reader(cpu, event.id(), config().mmap_pages), event_(event),
  writer_(trace_.metric_writer((boost::format("tracepoint metrics for CPU %d") % cpu).str())),
  metric_instance_(
      trace_.metric_instance(metric_class, writer_.location(), trace_.system_tree_cpu_node(cpu))),
  time_converter_(perf::time::Converter::instance())
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

    size_t index = 0;
    for (const auto& field : event_.fields())
    {
        if (!field.is_integer())
        {
            continue;
        }
        counter_values_.at(index++).set(sample->raw_data.get(field));
    }
    writer_.write(otf2::event::metric(tp, metric_instance_, counter_values_));
    return false;
}
}
}
}
