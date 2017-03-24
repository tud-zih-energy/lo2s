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
    assert(stop_requested_);
    if (!finished())
    {
        Log::error() << "Trying to join non-finished monitor " << name() << ". "
                     << "That might take a while.";
    }
}

void ActiveMonitor::stop()
{
    std::unique_lock<std::mutex> lock(control_mutex_);
    stop_requested_ = true;
    control_condition_.notify_all();
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
