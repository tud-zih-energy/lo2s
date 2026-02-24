/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <lo2s/ompt/events.hpp>
#include <lo2s/ompt/ringbuf.hpp>
#include <lo2s/types/process.hpp>

#include <memory>

#include <cstdint>

#include <omp-tools.h>
#include <unistd.h>

namespace
{
std::unique_ptr<lo2s::ompt::RingbufWriter> ompt_rb_writer = nullptr;

void on_ompt_callback_parallel_begin(ompt_data_t* /*parent_task_data*/,
                                     const ompt_frame_t* /*parent_task_frame*/,
                                     ompt_data_t* /*parallel_data*/, uint32_t requested_team_size,
                                     int /*flag*/, const void* codeptr_ra)
{

    struct lo2s::ompt::OMPTCctx cctx;

    cctx.type = lo2s::ompt::OMPType::PARALLEL;
    cctx.addr = reinterpret_cast<uint64_t>(codeptr_ra);
    cctx.tid = gettid();
    cctx.num_threads = requested_team_size;

    ompt_rb_writer->ompt_enter(ompt_rb_writer->timestamp(), cctx);
}

void on_ompt_callback_parallel_end(ompt_data_t* /*parallel_data*/, ompt_data_t* /*task_data*/,
                                   int /*flag*/, const void* codeptr_ra)
{
    struct lo2s::ompt::OMPTCctx cctx;

    cctx.type = lo2s::ompt::OMPType::PARALLEL;
    cctx.addr = reinterpret_cast<uint64_t>(codeptr_ra);
    cctx.tid = gettid();

    ompt_rb_writer->ompt_leave(ompt_rb_writer->timestamp(), cctx);
}

void on_ompt_callback_master(ompt_scope_endpoint_t endpoint, ompt_data_t* /*parallel_data*/,
                             ompt_data_t* /*task_data*/, const void* codeptr_ra)
{
    struct lo2s::ompt::OMPTCctx cctx;

    cctx.type = lo2s::ompt::OMPType::MASTER;
    cctx.addr = reinterpret_cast<uint64_t>(codeptr_ra);
    cctx.tid = gettid();

    if (endpoint == ompt_scope_begin)
    {
        ompt_rb_writer->ompt_enter(ompt_rb_writer->timestamp(), cctx);
    }
    else if (endpoint == ompt_scope_end)
    {
        ompt_rb_writer->ompt_leave(ompt_rb_writer->timestamp(), cctx);
    }
}

void on_ompt_callback_work(ompt_work_t wstype, ompt_scope_endpoint_t endpoint,
                           ompt_data_t* /*parallel_data*/, ompt_data_t* /*task_data*/,
                           uint64_t /*count*/, const void* codeptr_ra)
{
    struct lo2s::ompt::OMPTCctx cctx;

    switch (wstype)
    {
    case ompt_work_loop:
    case ompt_work_loop_static:
        cctx.type = lo2s::ompt::OMPType::LOOP;
        break;
    case ompt_work_workshare:
        cctx.type = lo2s::ompt::OMPType::WORKSHARE;
        break;
    default:
        cctx.type = lo2s::ompt::OMPType::OTHER;
        break;
    }

    cctx.addr = reinterpret_cast<uint64_t>(codeptr_ra);
    cctx.tid = gettid();

    if (endpoint == ompt_scope_begin)
    {
        ompt_rb_writer->ompt_enter(ompt_rb_writer->timestamp(), cctx);
    }
    else if (endpoint == ompt_scope_end)
    {
        ompt_rb_writer->ompt_leave(ompt_rb_writer->timestamp(), cctx);
    }
}

void on_ompt_callback_sync_region(ompt_sync_region_t /*kind*/, ompt_scope_endpoint_t endpoint,
                                  ompt_data_t* /*parallel_data*/, ompt_data_t* /*task_data*/,
                                  const void* codeptr_ra)
{
    struct lo2s::ompt::OMPTCctx cctx;

    cctx.type = lo2s::ompt::OMPType::SYNC;
    cctx.addr = reinterpret_cast<uint64_t>(codeptr_ra);
    cctx.tid = gettid();

    if (endpoint == ompt_scope_begin)
    {
        ompt_rb_writer->ompt_enter(ompt_rb_writer->timestamp(), cctx);
    }
    else if (endpoint == ompt_scope_end)
    {
        ompt_rb_writer->ompt_leave(ompt_rb_writer->timestamp(), cctx);
    }
}

#define register_callback_t(name, type)                                                            \
    ompt_set_callback(name, reinterpret_cast<ompt_callback_t>(on_##name))

#define register_callback(name) register_callback_t(name, name##_t)

int ompt_initialize(ompt_function_lookup_t lookup, int /*initial_device_num*/,
                    ompt_data_t* /*tool_data*/)
{
    ompt_rb_writer = std::make_unique<lo2s::ompt::RingbufWriter>(lo2s::Process::me());
    auto ompt_set_callback = reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_master);
    register_callback(ompt_callback_work);
    register_callback(ompt_callback_sync_region);

    return 1;
}

void ompt_finalize(ompt_data_t* tool_data)
{
}

ompt_start_tool_result_t ompt_start_tool_result = { &ompt_initialize, &ompt_finalize, 0 };
} // namespace

extern "C" ompt_start_tool_result_t* ompt_start_tool(unsigned int /*omp_version*/,
                                                     const char* /*runtime_version*/)
{
    return &ompt_start_tool_result;
}
