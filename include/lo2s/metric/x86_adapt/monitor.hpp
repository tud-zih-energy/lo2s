#pragma once

#include <x86_adapt_cxx/configuration_item.hpp>
#include <x86_adapt_cxx/device.hpp>

#include <string>
#include <vector>
#ifndef HAVE_X86_ADAPT
#error "Trying to build x86 adapt stuff without x86 adapt support"
#endif

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_class.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

namespace lo2s::metric::x86_adapt
{
class Monitor : public monitor::PollMonitor
{
public:
    Monitor(::x86_adapt::device device,
            const std::vector<::x86_adapt::configuration_item>& configuration_items,
            trace::Trace& trace, const otf2::definition::metric_class& metric_class);

protected:
    void monitor(int fd) override;
    void initialize_thread() override;

    std::string group() const override
    {
        return "x86_adapt::Monitor";
    }

private:
    ::x86_adapt::device device_;

    otf2::writer::local& otf2_writer_;

    std::vector<::x86_adapt::configuration_item> configuration_items_;

    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric event_;
};
} // namespace lo2s::metric::x86_adapt
