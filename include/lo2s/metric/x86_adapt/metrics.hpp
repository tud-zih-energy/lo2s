#pragma once

#include <lo2s/metric/x86_adapt/recorder.hpp>

#include <lo2s/metric/guess_mode.hpp>

#include <lo2s/topology.hpp>
#include <lo2s/trace/fwd.hpp>

#include <x86_adapt_cxx/x86_adapt.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{
class Metrics
{
public:
    Metrics(trace::Trace& trace, std::chrono::nanoseconds sampling_interval,
            const std::vector<std::string>& cpu_items);

private:
    void start();

public:
    void stop();

private:
    ::x86_adapt::x86_adapt x86_adapt_;
    std::vector<std::unique_ptr<Recorder>> recorders_;
};
}
}
}
