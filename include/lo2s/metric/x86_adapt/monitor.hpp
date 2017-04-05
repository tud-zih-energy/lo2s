#pragma once

#ifndef HAVE_X86_ADAPT
#error "Trying to build x86 adapt stuff without x86 adapt support"
#endif

#include <lo2s/time/time.hpp>

#include <lo2s/monitor/interval_monitor.hpp>
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
class Monitor : public monitor::IntervalMonitor
{
public:
    Monitor(::x86_adapt::device device, std::chrono::nanoseconds sampling_interval,
            const std::vector<::x86_adapt::configuration_item>& configuration_items,
            trace::Trace& trace, const otf2::definition::metric_class& metric_class);

protected:
    void monitor() override;
    void initialize_thread() override;

    std::string group() const override
    {
        return "x86_adapt::Monitor";
    }

private:
    ::x86_adapt::device device_;

    otf2::writer::local otf2_writer_;

    std::vector<::x86_adapt::configuration_item> configuration_items_;

    otf2::definition::metric_instance metric_instance_;
    std::vector<otf2::event::metric::value_container> metric_values_;
};
}
}
}
