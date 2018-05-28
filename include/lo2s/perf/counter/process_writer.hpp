#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/perf/counter/abstract_writer.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
class ProcessWriter : public AbstractWriter
{
public:
    ProcessWriter(pid_t pid, pid_t tid, otf2::writer::local& writer, monitor::MainMonitor& parent,
                  otf2::definition::location scope, bool enable_on_exec);

private:
    void handle_custom_events(std::size_t position);
    boost::filesystem::ifstream proc_stat_;
};
} // namespace counter
} // namespace perf
} // namespace lo2s
