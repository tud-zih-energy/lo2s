#pragma once

#ifndef HAVE_X86_ENERGY
#error "Trying to build x86 energy stuff without x86 energy support"
#endif

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types.hpp>

#include <nitro/lang/enumerate.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>
#include <x86_energy.hpp>

namespace lo2s
{
namespace metric
{
namespace x86_energy
{
class Monitor : public monitor::PollMonitor
{
public:
    Monitor(::x86_energy::SourceCounter counter, Cpu cpu, trace::Trace& trace,
            const otf2::definition::metric_class& metric_class,
            const otf2::definition::system_tree_node& stn);

protected:
    void monitor(int fd) override;
    void initialize_thread() override;

    std::string group() const override
    {
        return "x86_energy::Monitor";
    }

private:
    ::x86_energy::SourceCounter counter_;

    Cpu cpu_;

    otf2::writer::local& otf2_writer_;

    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace x86_energy
} // namespace metric
} // namespace lo2s
