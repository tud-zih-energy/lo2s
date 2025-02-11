#include <lo2s/perf/syscall/writer.hpp>

#include <lo2s/trace/trace.hpp>

#include <fmt/core.h>

namespace lo2s
{
namespace perf
{
namespace syscall
{

Writer::Writer(ExecutionScope scope, trace::Trace& trace)
: Reader(scope), local_cctx_tree_(trace.create_local_cctx_tree(MeasurementScope::syscall(scope))),
  time_converter_(perf::time::Converter::instance())
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    if (sample->id == sys_enter_id)
    {
        local_cctx_tree_.cctx_enter(tp, CallingContext::syscall(sample->syscall_nr));
    }
    else
    {
        local_cctx_tree_.cctx_leave(tp, 1, CallingContext::syscall(sample->syscall_nr));
    }
    last_tp_ = tp;
    return false;
}

Writer::~Writer()
{
    local_cctx_tree_.cctx_leave(last_tp_, 0, CallingContext::root());

    local_cctx_tree_.finalize();
}
} // namespace syscall
} // namespace perf
} // namespace lo2s
