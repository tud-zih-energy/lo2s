#include <lo2s/perf/syscall/writer.hpp>

#include <lo2s/trace/trace.hpp>

#include <fmt/core.h>

namespace lo2s
{
namespace perf
{
namespace syscall
{

Writer::Writer(Cpu cpu, trace::Trace& trace)
: Reader(cpu), trace_(trace), time_converter_(perf::time::Converter::instance()),
  writer_(trace.syscall_writer(cpu)), last_syscall_nr_(-1), ctx_(trace.syscall_context(0))
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    if (sample->id == sys_enter_id)
    {
        if (last_syscall_nr_ != -1)
        {
            writer_ << otf2::event::calling_context_leave(tp, ctx_);
        }
        last_syscall_nr_ = sample->syscall_nr;
        writer_ << otf2::event::calling_context_enter(tp, ctx_, 2);
    }
    else
    {
        if (last_syscall_nr_ == sample->syscall_nr)
        {
            writer_ << otf2::event::calling_context_leave(tp, ctx_);
        }
        last_syscall_nr_ = -1;
    }
    return false;
}
} // namespace syscall
} // namespace perf
} // namespace lo2s
