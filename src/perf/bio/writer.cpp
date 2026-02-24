#include <lo2s/perf/bio/writer.hpp>

#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/common.hpp>
#include <otf2xx/definition/io_handle.hpp>
#include <otf2xx/writer/local.hpp>

#include <stdexcept>
#include <vector>

namespace lo2s::perf::bio
{
Writer::Writer(trace::Trace& trace) : trace_(trace), time_converter_(time::Converter::instance())
{
}

void Writer::write(IoReaderIdentity& identity, TracepointSampleType* header)
{
    if (identity.tracepoint() == bio_queue_)
    {
        auto* event = reinterpret_cast<RecordBioQueue*>(header);

        otf2::common::io_operation_mode_type mode = otf2::common::io_operation_mode_type::flush;
        // TODO: Handle the few io operations that arent either reads or write
        if (event->rwbs[0] == 'R')
        {
            mode = otf2::common::io_operation_mode_type::read;
        }
        else if (event->rwbs[0] == 'W')
        {
            mode = otf2::common::io_operation_mode_type::write;
        }
        else
        {
            return;
        }

        const BlockDevice dev = block_device_for<RecordBioQueue>(event);

        otf2::writer::local& writer = trace_.bio_writer(dev);
        const otf2::definition::io_handle& handle = trace_.block_io_handle(dev);
        auto size = event->nr_sector * SECTOR_SIZE;

        sector_cache_[dev][event->sector] += size;
        writer << otf2::event::io_operation_begin(
            time_converter_(event->header.time), handle, mode,
            otf2::common::io_operation_flag_type::non_blocking, size, event->sector);
    }
    else if (identity.tracepoint() == bio_issue_)
    {
        auto* event = reinterpret_cast<RecordBlock*>(header);

        const BlockDevice dev = block_device_for<RecordBlock>(event);

        if (sector_cache_.count(dev) == 0 || sector_cache_.at(dev).count(event->sector) == 0)
        {
            return;
        }

        otf2::writer::local& writer = trace_.bio_writer(dev);
        const otf2::definition::io_handle& handle = trace_.block_io_handle(dev);

        writer << otf2::event::io_operation_issued(time_converter_(event->header.time), handle,
                                                   event->sector);
    }
    else if (identity.tracepoint() == bio_complete_)
    {
        auto* event = reinterpret_cast<RecordBlock*>(header);

        const BlockDevice dev = block_device_for<RecordBlock>(event);

        if (sector_cache_.count(dev) == 0 && sector_cache_[dev].count(event->sector) == 0)
        {
            return;
        }

        otf2::writer::local& writer = trace_.bio_writer(dev);
        const otf2::definition::io_handle& handle = trace_.block_io_handle(dev);

        writer << otf2::event::io_operation_complete(time_converter_(event->header.time), handle,
                                                     sector_cache_[dev][event->sector],
                                                     event->sector);
        sector_cache_[dev][event->sector] = 0;
    }
    else
    {
        throw std::runtime_error("tracepoint " + identity.tracepoint().name() +
                                 " not valid for block I/O");
    }
}

std::vector<perf::tracepoint::TracepointEventAttr> Writer::get_tracepoints()
{
    bio_queue_ = perf::EventComposer::instance().create_tracepoint_event("block:block_bio_queue");
    bio_issue_ = perf::EventComposer::instance().create_tracepoint_event("block:block_rq_issue");
    bio_complete_ =
        perf::EventComposer::instance().create_tracepoint_event("block:block_rq_complete");

    return { bio_queue_.value(), bio_issue_.value(), bio_complete_.value() };
}
} // namespace lo2s::perf::bio
