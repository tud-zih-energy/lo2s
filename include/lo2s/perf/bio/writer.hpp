// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/trace/fwd.hpp>

#include <map>
#include <optional>
#include <vector>

#include <cstdint>

extern "C"
{
#include <sys/sysmacros.h>
}

namespace lo2s::perf::bio
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

struct __attribute((__packed__)) RecordBioQueue
{
    TracepointSampleType header;
    uint16_t common_type; // common fields included in all tracepoints, ignored here
    uint8_t common_flag;
    uint8_t common_preempt_count;
    int32_t common_pid;

    uint32_t dev;    // the accessed device (as dev_t)
    char padding[4]; // padding because of struct alignment
    uint64_t sector; // the accessed sector on the device

    uint32_t nr_sector; // the number of sector_cache_ written

    char rwbs[8]; // the type of the operation. "R" for read, "W" for write, etc.
};

class Writer
{
public:
    Writer(trace::Trace& trace);
    void write(IoReaderIdentity& identity, TracepointSampleType* header);
    std::vector<perf::tracepoint::TracepointEventAttr> get_tracepoints();

private:
    template <class T>
    BlockDevice block_device_for(T* event)
    {
        // Something seems to be broken about the dev_t dev field returned from the block_rq_*
        // tracepoints What works is extracting the major:minor numbers from the dev field with
        // bitshifts as documented in block/block_rq_*/format So first convert the dev field
        // into major:minor numbers and then use the makedev macro to get an unbroken dev_t.
        return BlockDevice::block_device_for(
            makedev(event->dev >> 20, event->dev & ((1U << 20) - 1)));
    }

    std::map<BlockDevice, std::map<uint64_t, uint64_t>> sector_cache_;
    trace::Trace& trace_;
    time::Converter& time_converter_;

    // Unavailable until get_tracepoints() is called
    std::optional<perf::tracepoint::TracepointEventAttr> bio_queue_;
    std::optional<perf::tracepoint::TracepointEventAttr> bio_issue_;
    std::optional<perf::tracepoint::TracepointEventAttr> bio_complete_;

    // The unit "sector" is always 512 bit large, regardless of the actual sector size of the device
    static constexpr int SECTOR_SIZE = 512;
};
} // namespace lo2s::perf::bio
