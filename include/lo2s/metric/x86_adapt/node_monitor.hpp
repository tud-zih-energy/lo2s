#pragma once

#ifndef HAVE_X86_ADAPT
#error "Trying to build x86 adapt stuff without x86 adapt support"
#endif

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>

#include <nitro/lang/enumerate.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>
#include <x86_adapt_cxx/x86_adapt.hpp>

#include <atomic>
#include <chrono>
#include <thread>

extern "C"
{
#include <sched.h>
}

namespace lo2s
{
namespace metric
{
namespace x86_adapt
{
class NodeMonitor : public monitor::PollMonitor
{
public:
    NodeMonitor(::x86_adapt::device device,
                const std::vector<::x86_adapt::configuration_item>& configuration_items,
                trace::Trace& trace, const otf2::definition::metric_class& metric_class);

protected:
    void monitor(int fd) override;
    void initialize_thread() override;

    std::string group() const override
    {
        return "x86_adapt::NodeMonitor";
    }

private:
    ::x86_adapt::device device_;

    otf2::writer::local& otf2_writer_;

    std::vector<::x86_adapt::configuration_item> configuration_items_;

    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric event_;
};
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
