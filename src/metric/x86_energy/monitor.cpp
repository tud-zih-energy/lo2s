#include <lo2s/metric/x86_energy/monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <string>

namespace lo2s
{
namespace metric
{
namespace x86_energy
{

Monitor::Monitor(::x86_energy::SourceCounter counter, int cpu,
                 std::chrono::nanoseconds sampling_interval, trace::Trace& trace,
                 const otf2::definition::metric_class& metric_class,
                 const otf2::definition::system_tree_node& stn)
: IntervalMonitor(trace, std::to_string(cpu), sampling_interval), counter_(std::move(counter)),
  cpu_(cpu), otf2_writer_(trace.metric_writer(name())),
  metric_instance_(trace.metric_instance(metric_class, otf2_writer_.location(), stn)),
  metric_event_(otf2::chrono::genesis(), metric_instance_)
// (WOW this is (almost) better than LISP)
{
}

void Monitor::initialize_thread()
{
    try_pin_to_cpu(cpu_);
}

void Monitor::monitor()
{
    metric_event_.timestamp(time::now());
    metric_event_.raw_values()[0] = counter_.read();

    otf2_writer_.write(metric_event_);
}
} // namespace x86_energy
} // namespace metric
} // namespace lo2s
