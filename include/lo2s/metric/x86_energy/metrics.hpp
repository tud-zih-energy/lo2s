#pragma once

#include <memory>
#include <vector>
#ifndef HAVE_X86_ENERGY
#error "Trying to build x86 energy stuff without x86 energy support"
#endif

#include <lo2s/metric/x86_energy/monitor.hpp>
#include <lo2s/trace/fwd.hpp>

#include <x86_energy.hpp>

namespace lo2s::metric::x86_energy
{
class Metrics
{
public:
    Metrics(trace::Trace& trace);

    void start();
    void stop();

private:
    ::x86_energy::Architecture architecture_;
    std::vector<std::unique_ptr<Monitor>> recorders_;
};
} // namespace lo2s::metric::x86_energy
