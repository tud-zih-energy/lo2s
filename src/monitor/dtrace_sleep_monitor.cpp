#include <lo2s/monitor/dtrace_sleep_monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>

#include <cassert>

extern "C"
{
#include <dtrace.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{
namespace monitor
{

DtraceSleepMonitor::DtraceSleepMonitor(trace::Trace& trace, const std::string& name)
: ThreadedMonitor(trace, name), stop_requested_(false), handle(nullptr)
{
}

DtraceSleepMonitor::~DtraceSleepMonitor()
{
    // If we had an exception in the initialization list of ThreadMonitor (e.g. Writer() fails
    // because no more mappable memory is available thread_ will not be joinable (and not needed to
    // be stopped) and stop_requested should be false
    if (!stop_requested_ && thread_.joinable())
    {
        Log::error()
            << "Destructing DtraceSleepMonitor before being stopped. This should not happen, "
               "but it's fine anyway.";
        stop();
    }
}

void DtraceSleepMonitor::stop()
{
    stop_requested_ = true;

    try
    {
        assert(thread_.joinable());
        thread_.join();
    }
    catch (std::system_error& err)
    {
        Log::error() << "Failed to join DtraceSleepMonitor thread: " << err.what()
                     << " This is probably bad, we can't do anything about it.";
    }
}

void DtraceSleepMonitor::run()
{
    initialize_thread();

    do
    {
        Log::trace() << "Monitoring thread active.";
        monitor();
        num_wakeups_++;

        dtrace_sleep(handle);

    } while (!stop_requested_);

    dtrace_stop(handle);

    monitor();

    Log::debug() << "Monitoring thread finished";
}
} // namespace monitor
} // namespace lo2s
