#pragma once

#ifndef HAVE_X86_ENERGY
#error "Trying to build x86 energy stuff without x86 energy support"
#endif

#include <lo2s/time/time.hpp>

#include <lo2s/monitor/interval_monitor.hpp>
#include <lo2s/trace/fwd.hpp>

#include <x86_energy.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>

#include <nitro/lang/enumerate.hpp>

namespace lo2s
{
namespace metric
{
namespace x86_energy
{
class Monitor : public monitor::IntervalMonitor
{
public:
    Monitor(::x86_energy::SourceCounter counter, int cpu,
            std::chrono::nanoseconds sampling_interval, trace::Trace& trace,
            const otf2::definition::metric_class& metric_class,
            const otf2::definition::system_tree_node& stn);

protected:
    void monitor() override;
    void initialize_thread() override;

    std::string group() const override
    {
        return "x86_energy::Monitor";
    }

private:
    ::x86_energy::SourceCounter counter_;

    int cpu_;

    otf2::writer::local otf2_writer_;

    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric::value_container metric_value_;
};
} // namespace x86_energy
} // namespace metric
} // namespace lo2s
