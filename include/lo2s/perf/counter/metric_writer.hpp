#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <otf2xx/otf2.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
class MetricWriter
{
public:
    MetricWriter(ExecutionScope scope, trace::Trace& trace)
    : time_converter_(time::Converter::instance()), writer_(trace.metric_writer(scope)),
      metric_instance_(trace.metric_instance(trace.perf_metric_class(), writer_.location(),
                                             trace.location(scope))),
      metric_event_(otf2::chrono::genesis(), metric_instance_)
    {
    }

protected:
    time::Converter time_converter_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace counter
} // namespace perf
} // namespace lo2s
