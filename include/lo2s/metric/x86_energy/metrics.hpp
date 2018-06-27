#pragma once

#ifndef HAVE_X86_ENERGY
#error "Trying to build x86 energy stuff without x86 energy support"
#endif

#include <lo2s/metric/x86_energy/monitor.hpp>

#include <lo2s/topology.hpp>
#include <lo2s/trace/fwd.hpp>

#include <x86_energy.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace x86_energy
{
class Metrics
{
public:
    Metrics(trace::Trace& trace, std::chrono::nanoseconds sampling_interval);

public:
    void start();
    void stop();

private:
    ::x86_energy::Architecture architecture_;
    std::vector<std::unique_ptr<Monitor>> recorders_;
};
} // namespace x86_energy
} // namespace metric
} // namespace lo2s
