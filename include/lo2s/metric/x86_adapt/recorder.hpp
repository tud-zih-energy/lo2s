#pragma once

#include <lo2s/time/time.hpp>

#include <lo2s/trace/fwd.hpp>

#include <x86_adapt_cxx/x86_adapt.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>

#include <nitro/lang/enumerate.hpp>

#include <atomic>
#include <chrono>
#include <thread>

extern "C" {
#include <sched.h>
}

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{
class Recorder
{
public:
    Recorder(::x86_adapt::device device, std::chrono::nanoseconds sampling_interval,
             const std::vector<::x86_adapt::configuration_item>& configuration_items,
             trace::Trace& trace, const otf2::definition::metric_class& metric_class);

    void loop();

    void start();

    void stop();

private:
    bool running_ = false;
    ::x86_adapt::device device_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> looping_;

    std::chrono::nanoseconds sampling_interval_;

    otf2::writer::local writer_;
    otf2::definition::metric_instance metric_instance_;

    std::vector<::x86_adapt::configuration_item> configuration_items_;
};
}
}
}
