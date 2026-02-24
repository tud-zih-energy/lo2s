#include <lo2s/perf/syscall/writer.hpp>

#include <lo2s/calling_context.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/syscall/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/exception.hpp>

#include <fmt/core.h>

namespace lo2s::perf::syscall
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
        local_cctx_tree_.cctx_enter(tp, CCTX_LEVEL_SYSCALL,
                                    CallingContext::syscall(sample->syscall_nr));
    }
    else
    {
        local_cctx_tree_.cctx_leave(tp, CCTX_LEVEL_SYSCALL);
    }

    last_tp_ = tp;
    return false;
}

Writer::~Writer()
{
    try
    {
        local_cctx_tree_.cctx_leave(last_tp_, 1);
    }
    catch (otf2::exception& e)
    {
        Log::error() << "Can not write final syscall leave event, your trace might be broken!";
    }
    local_cctx_tree_.finalize();
}
} // namespace lo2s::perf::syscall
