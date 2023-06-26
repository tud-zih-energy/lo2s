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

#include <filesystem>
#include <regex>
extern "C"
{
#include <sys/sysmacros.h>
}
namespace lo2s
{
namespace perf
{
namespace nvme
{
// Adapted from the NVMe Specifications (https://nvmexpress.org/specifications)

enum class NVMeOp
{
    FLUSH = 0,
    READ = 1,
    WRITE = 2
};

struct __attribute__((packed)) RecordNVMeSetupCmd
{
    TracepointSampleType header;
    uint16_t common_type; // common fields included in all tracepoints, ignored here
    uint8_t common_flag;
    uint8_t common_preempt_count;
    int32_t common_pid;

    char disk[32];     // Name of the disk, e.g. /dev/nvme...
    int32_t ctrl_id;   //
    int32_t qid;       //  Queue Id, qid=0 is the admin command queue
    uint8_t opcode;    //  The opcode of the NVMe command
    uint8_t flags;     //
    uint8_t fctype;    //  NVMe over fabric specific information
    uint8_t alignment; //
    uint16_t cid;      // Command identifier. "This shall be unique amonst all outstanding commands"
    uint16_t more_alignment; // Because it is totally sane to have a tracepoint format where
                             // everything but the last field is aligned
    uint32_t nsid;           // Namespace Identifier
    uint8_t metadata;
    uint8_t cdw10[24]; // NVMe Command specific data
};

struct __attribute__((packed)) nvme_rw
{
    uint64_t slba;   // starting logical block address
    uint16_t length; // length in logical blocks
};

struct __attribute__((packed)) RecordNVMeCompleteRq
{
    TracepointSampleType header;
    uint16_t common_type; // common fields included in all tracepoints, ignored here
    uint8_t common_flag;
    uint8_t common_preempt_count;
    int32_t common_pid;

    char disk[32];
    int32_t ctrl_id;
    int32_t qid;
    int32_t cid;
    uint32_t alignment;
    uint64_t result;
    uint8_t retries;
    uint8_t flags;
    uint16_t status;
};

class Writer
{
public:
    Writer(trace::Trace& trace) : trace_(trace), time_converter_(time::Converter::instance())
    {
    }

    void write(IoReaderIdentity identity, TracepointSampleType* header)
    {

        uint64_t matching_id;
        uint16_t num_blocks = 0;
        std::string name;
        otf2::common::io_operation_mode_type mode = otf2::common::io_operation_mode_type::flush;

        if (identity.tp == nvme_setup_cmd_)
        {

            struct RecordNVMeSetupCmd* event = (RecordNVMeSetupCmd*)header;

            // Queue 0 is for administratvie commands, so ignore it
            if (event->qid != 0)
            {
                if (event->opcode == static_cast<uint8_t>(NVMeOp::READ))
                {
                    mode = otf2::common::io_operation_mode_type::read;
                }
                else if (event->opcode == static_cast<uint8_t>(NVMeOp::WRITE))
                {
                    mode = otf2::common::io_operation_mode_type::write;
                }
                else
                {
                    // TODO: Handle commands other than READ/Write, especially "write zeroes"
                    return;
                }

                struct nvme_rw* rw_command = (struct nvme_rw*)event->cdw10;
                matching_id = event->cid;
                name = event->disk;
                num_blocks = rw_command->length;

                size_cache.insert_or_assign(matching_id, num_blocks);
            }
            else
            {
                return;
            }
        }
        else if (identity.tp == nvme_complete_rq_)
        {
            struct RecordNVMeCompleteRq* event = (RecordNVMeCompleteRq*)header;
            name = event->disk;
            matching_id = event->cid;

            if (size_cache.count(matching_id) != 0)
            {
                num_blocks = size_cache.at(matching_id);
                size_cache.erase(matching_id);
            }
        }
        else
        {
            throw std::runtime_error("tracepoint " + std::to_string(identity.tp) +
                                     "not valid for NVMe tracing!");
        }

        BlockDevice dev = BlockDevice::block_device_for(dev_for_nvme(name));

        otf2::writer::local& writer = trace_.bio_writer(dev);
        otf2::definition::io_handle& handle = trace_.block_io_handle(dev);

        if (identity.tp == nvme_setup_cmd_)
        {
            writer << otf2::event::io_operation_begin(
                time_converter_(header->time), handle, mode,
                otf2::common::io_operation_flag_type::non_blocking, num_blocks, matching_id);
        }
        else if (identity.tp == nvme_complete_rq_)
        {
            writer << otf2::event::io_operation_complete(time_converter_(header->time), handle,
                                                         num_blocks, matching_id);
        }
        else
        {
            throw std::runtime_error("tracepoint " + std::to_string(identity.tp) +
                                     "not valid for NVMe tracing!");
        }
    }

    std::vector<int> get_tracepoints()
    {

        nvme_setup_cmd_ = tracepoint::EventFormat("nvme:nvme_setup_cmd").id();
        nvme_complete_rq_ = tracepoint::EventFormat("nvme:nvme_complete_rq").id();
        return { nvme_setup_cmd_, nvme_complete_rq_ };
    }

private:
    dev_t dev_for_nvme(std::string name)
    {
        const std::regex major_regex("MAJOR=(\\S+)");
        const std::regex minor_regex("MINOR=(\\S+)");

        std::smatch major_match;
        std::smatch minor_match;

        std::filesystem::path uevent_path = std::filesystem::path("/sys/block/") / name / "uevent";

        std::ifstream uevent_file(uevent_path);
        uint32_t major = 0;
        uint32_t minor = 0;
        while (uevent_file.good())
        {
            std::string line;
            uevent_file >> line;
            if (std::regex_match(line, major_match, major_regex))
            {
                major = std::stoi(major_match[1].str());
            }
            else if (std::regex_match(line, minor_match, minor_regex))
            {
                minor = std::stoi(minor_match[1].str());
            }
        }
        return makedev(major, minor);
    }

    std::size_t logical_block_size(std::string name)
    {
        std::filesystem::path lba_path =
            std::filesystem::path("/sys/class/block") / name / "queue/logical_block_size";
        std::ifstream lba_file(lba_path);

        if (lba_file.good())
        {
            std::size_t lba;
            lba_file >> lba;

            return lba;
        }
        Log::warn() << "Can not detect logical block size, assuming 512 byte!";

        return 512;
    }
    trace::Trace& trace_;
    time::Converter& time_converter_;

    int nvme_setup_cmd_;
    int nvme_complete_rq_;
    std::map<uint64_t, uint64_t> size_cache;
};
} // namespace nvme
} // namespace perf
} // namespace lo2s
