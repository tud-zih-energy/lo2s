#include <lo2s/perf/counter/process_writer.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
ProcessWriter::ProcessWriter(pid_t pid, pid_t tid, otf2::writer::local& writer, monitor::MainMonitor& parent, otf2::definition::location scope, bool enable_on_exec)
: AbstractWriter(tid, -1, writer, parent.trace().metric_instance(parent.get_metric_class(), writer.location(), scope), enable_on_exec),
proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) / "stat")
{
}

void ProcessWriter::handle_custom_events(std::size_t position)
{
    values_[position].set(get_task_last_cpu_id(proc_stat_));
}
}
}
}
