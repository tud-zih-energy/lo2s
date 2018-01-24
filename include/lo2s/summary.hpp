#include <chrono>
#include <mutex>
#include <unordered_set>
#include <atomic>

extern "C" {
#include <sys/types.h>
}

namespace lo2s
{

class Summary
{
public:
    void show();

    void add_thread();
    void register_process(pid_t pid);

    void record_perf_wakeups(std::size_t num_wakeups);

    void set_exit_code(int exit_code);
    void set_trace_dir(const std::string trace_dir);

    friend Summary& summary();

private:
    Summary();

    std::chrono::steady_clock::time_point start_wall_time_;

    std::atomic<std::size_t> num_wakeups_;
    std::atomic<std::size_t> thread_count_;

    std::unordered_set<pid_t> pids_;
    std::mutex pids_mutex_;

    std::string trace_dir_;

    int exit_code_;
};

Summary& summary();
}
