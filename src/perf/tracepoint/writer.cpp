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
  writer_(trace_.named_metric_writer((boost::format("tracepoint metrics for CPU %d") % cpu).str())),
  metric_instance_(
      trace_.metric_instance(metric_class, writer_.location(), trace_.system_tree_cpu_node(cpu))),
  time_converter_(perf::time::Converter::instance()),
  metric_event_(otf2::chrono::genesis(), metric_instance_)
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    metric_event_.timestamp(time_converter_(sample->time));

    std::size_t index = 0;
    for (const auto& field : event_.fields())
    {
        if (!field.is_integer())
        {
            continue;
        }

        metric_event_.raw_values()[index++] = sample->raw_data.get(field);
    }
    writer_.write(metric_event_);
    return false;
}
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
