#pragma once

#include <lo2s/util.hpp>

namespace lo2s
{
namespace perf
{
static inline int perf_event_paranoid()
{
    try
    {
        return get_sysctl<int>("kernel", "perf_event_paranoid");
    }
    catch (...)
    {
        Log::warn() << "Failed to access kernel.perf_event_paranoid. Assuming 2.";
        return 2;
    }
}
}
}
