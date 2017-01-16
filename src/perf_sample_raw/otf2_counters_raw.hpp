#ifndef LO2S_OTF2_COUNTERS_RAW_HPP
#define LO2S_OTF2_COUNTERS_RAW_HPP

#include "perf_sample_raw_otf2.hpp"

#include "../otf2_trace.hpp"
#include "../time.hpp"
#include "../topology.hpp"

#include <boost/format.hpp>

namespace lo2s
{
/*
 * NOTE: Encapsulates counters for ALL cpus, totally different than otf2_counters!
 */
class otf2_counters_raw
{
public:
    otf2_counters_raw(otf2_trace& trace, const perf_time_converter& time_converter)
    {
        auto mc = trace.metric_class();
        mc.add_member(trace.metric_member("cstate", "C state",
                                          otf2::common::metric_mode::absolute_next,
                                          otf2::common::type::int64, "cpuid"));

        //perf_recorders_.reserve(topology::instance().cpus().size());
        for (const auto& cpu : topology::instance().cpus())
        {
            perf_recorders_.emplace_back(cpu.id, trace, mc, time_converter);
        }
    }

private:
    std::vector<perf_sample_raw_otf2> perf_recorders_;
};
}

#endif // LO2S_OTF2_COUNTERS_RAW_HPP
