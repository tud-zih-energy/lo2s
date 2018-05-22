#include <lo2s/perf/counter/abstract_writer.hpp>
#include <lo2s/monitor/main_monitor.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
class CpuWriter : public AbstractWriter
{
public:
    CpuWriter(int cpuid, otf2::writer::local& writer, monitor::MainMonitor& parent, otf2::definition::location scope)
    : AbstractWriter(-1, cpuid, writer, parent.trace().metric_instance(parent.get_metric_class(), writer.location(), scope), false)
    {
    }
private:
    void handle_custom_events()
    {
    }
};
}
}
}
