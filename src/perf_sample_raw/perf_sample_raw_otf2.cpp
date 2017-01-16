#include <boost/format.hpp>

#include "../otf2_trace.hpp"

#include "perf_sample_raw_otf2.hpp"

namespace lo2s
{

perf_sample_raw_otf2::perf_sample_raw_otf2(int cpu, otf2_trace& trace,
                                           const otf2::definition::metric_class& metric_class,
                                           const perf_time_converter& time_converter)
: perf_sample_raw_reader(cpu),
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

bool perf_sample_raw_otf2::handle(const perf_sample_raw_reader::record_sample_type* sample)
{
    auto tp = time_converter_(sample->time);
    counter_values_[0].set(sample->data.state);
    writer_.write(otf2::event::metric(tp, metric_instance_, counter_values_));
    return false;
}

perf_sample_raw_otf2::~perf_sample_raw_otf2()
{
    this->read();
}
}
