#include <lo2s/monitor/active_monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>

#include <cassert>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{
namespace monitor
{

ActiveMonitor::ActiveMonitor(Monitor& parent_monitor, std::chrono::nanoseconds interval)
: parent_monitor_(parent_monitor), interval_(interval)
{
}

ActiveMonitor::~ActiveMonitor()
{
    std::unique_lock<std::mutex> lock(control_mutex_);
    if (!stop_requested_)
    {
        Log::error() << "Destructing ActiveMonitor before being stopped. This should not happen, "
                        "but it's fine anyway.";
        stop();
    }
}

void ActiveMonitor::stop()
{
    {
        std::unique_lock<std::mutex> lock(control_mutex_);
        assert(!stop_requested_);
        stop_requested_ = true;
        control_condition_.notify_all();
    }
    try
    {
        assert(thread_.joinable());
        thread_.join();
    }
    catch (std::system_error& err)
    {
        Log::error() << "Failed to join ActiveMonitor thread: " << err.what()
                     << " This is probably bad, we can't do anything about it.";
    }
}

void ActiveMonitor::start()
{
    assert(!thread_.joinable());
    thread_ = std::thread([this]() { this->run(); });
}

void ActiveMonitor::run()
{
    Log::info() << "New monitoring thread " << name() << " with interval of "
                << std::chrono::duration_cast<std::chrono::milliseconds>(interval_).count()
                << " ms and pid " << getpid() << ".";

    initialize_thread();

    auto deadline = time::Clock::now();
    // Move deadline to be the same for all thread, reducing noise imbalances
    deadline -= (deadline.time_since_epoch() % interval_);

    std::unique_lock<std::mutex> lock(control_mutex_);
    do
    {
        Log::trace() << "Monitoring thread " << name() << " active.";
        monitor();

        // TODO skip samples if we cannot keep up with the deadlines
        deadline += interval_;
        // TODO wait here for overflows in the sampling buffers in addition to the deadline!
    } while (!control_condition_.wait_until(lock, deadline, [this]() { return stop_requested_; }));

    monitor();
    finalize_thread();

    Log::debug() << "Monitoring thread " << name() << " finished";
    finished_.store(true);
}
}
}
