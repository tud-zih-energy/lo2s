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

#include <lo2s/ringbuf.hpp>

#include <omp-tools.h>
#include <omp.h>

#define REGION_OMP_PARALLEL "!$omp parallel"

std::unique_ptr<lo2s::RingbufWriter> ompt_rb_writer = nullptr;
static ompt_set_callback_t ompt_set_callback;

static void on_ompt_callback_parallel_begin(ompt_data_t* parent_task_data,
                                            const ompt_frame_t* parent_task_frame,
                                            ompt_data_t* parallel_data,
                                            uint32_t requested_team_size, int flag,
                                            const void* codeptr_ra)
{
    ompt_rb_writer->cctx_enter(ompt_rb_writer->timestamp(),
                               ompt_rb_writer->cctx_def(REGION_OMP_PARALLEL));
}

static void on_ompt_callback_parallel_end(ompt_data_t* parallel_data, ompt_data_t* task_data,
                                          int flag, const void* codeptr_ra)
{

    ompt_rb_writer->cctx_leave(ompt_rb_writer->timestamp(),
                               ompt_rb_writer->cctx_def(REGION_OMP_PARALLEL));
}

#define register_callback_t(name, type) ompt_set_callback(name, (ompt_callback_t)on_##name)

#define register_callback(name) register_callback_t(name, name##_t)

int ompt_initialize(ompt_function_lookup_t lookup, int initial_device_num, ompt_data_t* tool_data)
{
    pid_t pid = getpid();
    ompt_rb_writer =
        std::make_unique<lo2s::RingbufWriter>(lo2s::ExecutionScope(lo2s::Process(pid)), "ompt");
    ompt_set_callback = (ompt_set_callback_t)lookup("ompt_set_callback");
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);

    ompt_rb_writer->cctx_def(REGION_OMP_PARALLEL);
    return 1;
}

void ompt_finalize(ompt_data_t* tool_data)
{
}

extern "C" ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version,
                                                     const char* runtime_version)
{
    printf("HELLO WORLD!\n");
    if (omp_version != _OPENMP)
        printf("Warning: OpenMP runtime version (%i) "
               "does not match the compile time version (%i)"
               " for runtime identifying as %s\n",
               omp_version, _OPENMP, runtime_version);
    // Returning NULL will disable this as an OMPT tool,
    // allowing other tools to be loaded
    static ompt_start_tool_result_t ompt_start_tool_result = { &ompt_initialize, &ompt_finalize,
                                                               0 };
    return &ompt_start_tool_result;
    return NULL;
}
