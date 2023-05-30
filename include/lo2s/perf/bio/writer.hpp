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

#include <lo2s/perf/bio/reader.hpp>
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
class Writer
{
public:
    Writer(trace::Trace& trace) : trace_(trace), time_converter_(time::Converter::instance())
    {
    }

    void write(Reader::IdentityType identity, RecordBlockSampleType* header)
    {
        struct RecordBlockSampleType* event = (RecordBlockSampleType*)header;
        struct RecordBlock* record = (struct RecordBlock*)&(event->blk);

        otf2::common::io_operation_mode_type mode = otf2::common::io_operation_mode_type::flush;
        // TODO: Handle the few io operations that arent either reads or write
        if (record->rwbs[0] == 'R')
        {
            mode = otf2::common::io_operation_mode_type::read;
        }
        else if (record->rwbs[0] == 'W')
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
        BlockDevice dev = BlockDevice::block_device_for(
            makedev(record->dev >> 20, record->dev & ((1U << 20) - 1)));

        otf2::writer::local& writer = trace_.bio_writer(dev);
        otf2::definition::io_handle& handle = trace_.block_io_handle(dev);

        auto size = record->nr_sector * SECTOR_SIZE;

        if (identity.type == BioEventType::INSERT)
        {
            writer << otf2::event::io_operation_begin(
                time_converter_(event->time), handle, mode,
                otf2::common::io_operation_flag_type::non_blocking, size, record->sector);
        }
        else if (identity.type == BioEventType::ISSUE)
        {
            writer << otf2::event::io_operation_issued(time_converter_(event->time), handle,
                                                       record->sector);
        }
        else
        {
            writer << otf2::event::io_operation_complete(time_converter_(event->time), handle, size,
                                                         record->sector);
        }
    }

private:
    trace::Trace& trace_;
    time::Converter& time_converter_;

    // The unit "sector" is always 512 bit large, regardless of the actual sector size of the device
    static constexpr int SECTOR_SIZE = 512;
};
} // namespace bio
} // namespace perf
} // namespace lo2s
