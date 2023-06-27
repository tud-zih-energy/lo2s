/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

extern "C"
{
#include <sys/sysmacros.h>
}
namespace lo2s
{
namespace perf
{
namespace bio
{
struct __attribute((__packed__)) RecordBlock
{
    TracepointSampleType header;
    uint16_t common_type; // common fields included in all tracepoints, ignored here
    uint8_t common_flag;
    uint8_t common_preempt_count;
    int32_t common_pid;

    uint32_t dev;    // the accessed device (as dev_t)
    char padding[4]; // padding because of struct alignment
    uint64_t sector; // the accessed sector on the device

    uint32_t nr_sector;     // the number of sectors written
    int32_t error_or_bytes; // for insert/issue: number of bytes written (nr_sector * 512)
                            // for complete: the error code of the operation

    char rwbs[8]; // the type of the operation. "R" for read, "W" for write, etc.
};

class Writer
{
public:
    Writer(trace::Trace& trace) : trace_(trace), time_converter_(time::Converter::instance())
    {
    }

    void write(IoReaderIdentity identity, TracepointSampleType* header)
    {
        struct RecordBlock* event = (RecordBlock*)header;
        Log::warn() << "BLA" << event->rwbs;

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

        // Something seems to be broken about the dev_t dev field returned from the block_rq_*
        // tracepoints What works is extracting the major:minor numbers from the dev field with
        // bitshifts as documented in block/block_rq_*/format So first convert the dev field into
        // major:minor numbers and then use the makedev macro to get an unbroken dev_t.
        BlockDevice dev =
            BlockDevice::block_device_for(makedev(event->dev >> 20, event->dev & ((1U << 20) - 1)));

        otf2::writer::local& writer = trace_.bio_writer(dev);
        otf2::definition::io_handle& handle = trace_.block_io_handle(dev);

        auto size = event->nr_sector * SECTOR_SIZE;

        if (identity.tp == bio_insert_)
        {
            writer << otf2::event::io_operation_begin(
                time_converter_(event->header.time), handle, mode,
                otf2::common::io_operation_flag_type::non_blocking, size, event->sector);
        }
        else if (identity.tp == bio_issue_)
        {
            writer << otf2::event::io_operation_issued(time_converter_(event->header.time), handle,
                                                       event->sector);
        }
        else if (identity.tp == bio_complete_)
        {
            writer << otf2::event::io_operation_complete(time_converter_(event->header.time),
                                                         handle, size, event->sector);
        }
        else
        {
            throw std::runtime_error("tracepoint " + std::to_string(identity.tp) +
                                     "not valid for block I/O");
        }
    }

    std::vector<int> get_tracepoints()
    {

        bio_insert_ = tracepoint::EventFormat("block:block_rq_insert").id();
        bio_issue_ = tracepoint::EventFormat("block:block_rq_issue").id();
        bio_complete_ = tracepoint::EventFormat("block:block_rq_complete").id();

        return { bio_insert_, bio_issue_, bio_complete_ };
    }

    void finalize()
    {
    }

private:
    trace::Trace& trace_;
    time::Converter& time_converter_;

    int bio_insert_;
    int bio_issue_;
    int bio_complete_;
    // The unit "sector" is always 512 bit large, regardless of the actual sector size of the device
    static constexpr int SECTOR_SIZE = 512;
};
} // namespace bio
} // namespace perf
} // namespace lo2s
