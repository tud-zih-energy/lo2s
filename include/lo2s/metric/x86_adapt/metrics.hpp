#pragma once

#ifndef HAVE_X86_ADAPT
#error "Trying to build x86 adapt stuff without x86 adapt support"
#endif

#include <lo2s/metric/x86_adapt/monitor.hpp>
#include <lo2s/metric/x86_adapt/node_monitor.hpp>

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
    Metrics(trace::Trace& trace, const std::vector<std::string>& items);

public:
    void start();
    void stop();

private:
    ::x86_adapt::x86_adapt x86_adapt_;
    std::vector<std::unique_ptr<Monitor>> recorders_;
    std::vector<std::unique_ptr<NodeMonitor>> node_recorders_;
};
} // namespace x86_adapt
} // namespace metric
} // namespace lo2s
