
#ifndef LO2S_PERF_SAMPLE_RAW_OTF2_HPP
#define LO2S_PERF_SAMPLE_RAW_OTF2_HPP

#include "perf_sample_raw_reader.hpp"

#include "../time.hpp"

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <vector>

namespace lo2s
{

class perf_sample_raw_otf2 : public perf_sample_raw_reader<perf_sample_raw_otf2>
{
public:
    perf_sample_raw_otf2(int cpu, otf2_trace& trace,
                         const otf2::definition::metric_class& metric_class,
                         const perf_time_converter& time_converter);

    perf_sample_raw_otf2(perf_sample_raw_otf2&& other) = default;
    /* will probably do this:
    : perf_sample_raw_reader<perf_sample_raw_otf2>(
          std::forward<perf_sample_raw_reader<perf_sample_raw_otf2>>(other)),
      writer_(other.writer_), metric_instance_(std::move(other.metric_instance_)),
      time_converter_(other.time_converter_), counter_values_(std::move(other.counter_values_))
    {
    }*/

    ~perf_sample_raw_otf2();

public:
    using perf_sample_raw_reader<perf_sample_raw_otf2>::handle;
    bool handle(const perf_sample_raw_reader::record_sample_type* sample);

private:
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;

    const perf_time_converter& time_converter_;

    std::vector<otf2::event::metric::value_container> counter_values_;
};
}

#endif // LO2S_PERF_SAMPLE_RAW_OTF2_HPP
