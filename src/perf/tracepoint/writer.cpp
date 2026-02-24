#include <lo2s/perf/tracepoint/writer.hpp>

#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/tracepoint/reader.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/cpu.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/definition/metric_class.hpp>

#include <cstddef>

#include <fmt/core.h>
#include <fmt/format.h>

namespace lo2s::perf::tracepoint
{

Writer::Writer(Cpu cpu, const perf::tracepoint::TracepointEventAttr& event, trace::Trace& trace_,
               const otf2::definition::metric_class& metric_class)
: Reader(cpu, event),
  writer_(trace_.create_metric_writer(fmt::format("tracepoint metrics for {}", cpu))),
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
    for (const auto& field : Reader<Writer>::event_.fields())
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
} // namespace lo2s::perf::tracepoint
