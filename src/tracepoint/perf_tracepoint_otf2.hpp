#ifndef LO2S_PERF_SAMPLE_RAW_OTF2_HPP
#define LO2S_PERF_SAMPLE_RAW_OTF2_HPP

#include "event_format.hpp"
#include "perf_tracepoint_reader.hpp"

#include "../monitor_config.hpp"
#include "../otf2_trace.hpp"
#include "../time/converter.hpp"

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <vector>

namespace lo2s
{

class perf_tracepoint_otf2 : public perf_tracepoint_reader<perf_tracepoint_otf2>
{
public:
    perf_tracepoint_otf2(int cpu, const event_format& event, const monitor_config& config,
                         otf2_trace& trace, const otf2::definition::metric_class& metric_class,
                         const perf_time_converter& time_converter);

    perf_tracepoint_otf2(const perf_tracepoint_otf2& other) = delete;

    perf_tracepoint_otf2(perf_tracepoint_otf2&& other) = default;

public:
    using perf_tracepoint_reader<perf_tracepoint_otf2>::handle;
    bool handle(const perf_tracepoint_reader::record_sample_type* sample);

private:
    event_format event_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;

    const perf_time_converter& time_converter_;

    std::vector<otf2::event::metric::value_container> counter_values_;
};
}

#endif // LO2S_PERF_SAMPLE_RAW_OTF2_HPP
