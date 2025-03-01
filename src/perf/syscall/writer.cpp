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
: Reader(scope), trace_(trace), time_converter_(perf::time::Converter::instance()),
  writer_(trace.syscall_writer(scope)), last_syscall_nr_(-1)
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    if (sample->id == sys_enter_id)
    {
        if (last_syscall_nr_ != -1)
        {
            writer_.write_calling_context_leave(tp, sample->syscall_nr);
        }

        last_syscall_nr_ = sample->syscall_nr;
        writer_.write_calling_context_enter(tp, sample->syscall_nr, 2);
        used_syscalls_.emplace(sample->syscall_nr);
    }
    else
    {
        if (last_syscall_nr_ == sample->syscall_nr)
        {
            writer_.write_calling_context_leave(tp, sample->syscall_nr);
        }
        else if (last_syscall_nr_ != -1)
        {
            writer_.write_calling_context_leave(tp, last_syscall_nr_);
        }

        last_syscall_nr_ = -1;
    }
    last_tp_ = tp;
    return false;
}

Writer::~Writer()
{
    if (last_syscall_nr_ != -1)
    {
        writer_.write_calling_context_leave(last_tp_, last_syscall_nr_);
    }

    if (!used_syscalls_.empty())
    {
        const auto& mapping = trace_.merge_syscall_contexts(used_syscalls_);
        writer_ << mapping;
    }
}
} // namespace syscall
} // namespace perf
} // namespace lo2s
