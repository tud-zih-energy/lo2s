/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2025,
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

#include <lo2s/gpu/ringbuf.hpp>

#include <queue>
#include <regex>

#include <rocprofiler-sdk/registration.h>
#include <rocprofiler-sdk/rocprofiler.h>

namespace
{
constexpr size_t HIP_BUFFER_SIZE = 8 * 1024 * 1024;

uint64_t last_timestamp = 0;
int64_t offset;

rocprofiler_buffer_id_t client_buffer;

std::unique_ptr<lo2s::gpu::RingbufWriter> rb_writer = nullptr;
struct kernel_cache
{
    kernel_cache(uint64_t kernel_id, uint64_t start_timestamp, uint64_t end_timestamp)
    : kernel_id(kernel_id), start_timestamp(start_timestamp), end_timestamp(end_timestamp)
    {
    }

    uint64_t kernel_id;
    uint64_t start_timestamp;
    uint64_t end_timestamp;

    friend bool operator<(const struct kernel_cache& lhs, const struct kernel_cache& rhs)
    {
        return lhs.start_timestamp > rhs.start_timestamp;
    }
};

// Unlike NVidia, with AMD you can not supply an alternative
// timestamp source and the clock source used, BOOTTIME,
// can lead to problems if used with perf.
//
// So instead, use this crude time synchronization.
int64_t calculate_offset()
{
    uint64_t roc_timestamp;
    rocprofiler_get_timestamp(&roc_timestamp);

    uint64_t lo2s_timestamp = rb_writer->timestamp();

    return roc_timestamp - lo2s_timestamp;
}
} // namespace

void tool_tracing_callback(rocprofiler_context_id_t context, rocprofiler_buffer_id_t buffer_id,
                           rocprofiler_record_header_t** headers, size_t num_headers,
                           void* user_data, uint64_t drop_count)
{
    std::priority_queue<struct kernel_cache> reorder_buffer;

    for (size_t i = 0; i < num_headers; ++i)
    {
        rocprofiler_record_header_t* header = headers[i];

        if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING &&
            header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH)
        {
            auto* record = reinterpret_cast<rocprofiler_buffer_tracing_kernel_dispatch_record_t*>(
                header->payload);

            reorder_buffer.emplace(record->dispatch_info.kernel_id,
                                   record->start_timestamp - offset,
                                   record->end_timestamp - offset);

            if (reorder_buffer.size() >= 100)
            {
                struct kernel_cache ev = reorder_buffer.top();
                reorder_buffer.pop();

                if (ev.start_timestamp >= last_timestamp)
                {
                    rb_writer->kernel(ev.start_timestamp, ev.end_timestamp, ev.kernel_id);
                    last_timestamp = ev.end_timestamp;
                }
                else
                {
                    std::cerr << "HIP GPU kernel event arriving late!" << std::endl;
                }
            }
        }
    }

    while (!reorder_buffer.empty())
    {
        struct kernel_cache ev = reorder_buffer.top();
        reorder_buffer.pop();

        if (ev.start_timestamp >= last_timestamp)
        {
            rb_writer->kernel(ev.start_timestamp, ev.end_timestamp, ev.kernel_id);
            last_timestamp = ev.end_timestamp;
        }
        else
        {
            std::cerr << "HIP GPU kernel event arriving late!!" << std::endl;
        }
    }
}

void code_object_cb(rocprofiler_callback_tracing_record_t record,
                    rocprofiler_user_data_t* user_data, void* callback_data)
{
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT)
    {
        if (record.operation == ROCPROFILER_CODE_OBJECT_LOAD &&
            record.phase == ROCPROFILER_CALLBACK_PHASE_UNLOAD)
        {
            rocprofiler_flush_buffer(client_buffer);
        }
        else if (record.operation == ROCPROFILER_CODE_OBJECT_DEVICE_KERNEL_SYMBOL_REGISTER)
        {
            auto* data = static_cast<
                rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t*>(
                record.payload);

            auto kernel_name = std::regex_replace(data->kernel_name, std::regex{ "(\\.kd)$" }, "");
            rb_writer->kernel_def(kernel_name, data->kernel_id);
        }
    }
}

int tool_init(rocprofiler_client_finalize_t fini_func, void* tool_data)
{
    rb_writer = std::make_unique<lo2s::gpu::RingbufWriter>(lo2s::Process::me());

    offset = calculate_offset();

    rocprofiler_context_id_t client_ctx = { 0 };
    rocprofiler_create_context(&client_ctx);

    rocprofiler_configure_callback_tracing_service(client_ctx,
                                                   ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
                                                   nullptr, 0, code_object_cb, tool_data);

    // Create buffer for asynchronous event recording, set watermark to 80% buffer size
    rocprofiler_create_buffer(client_ctx, HIP_BUFFER_SIZE, HIP_BUFFER_SIZE * 0.8,
                              ROCPROFILER_BUFFER_POLICY_LOSSLESS, tool_tracing_callback, tool_data,
                              &client_buffer);

    rocprofiler_configure_buffer_tracing_service(
        client_ctx, ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH, nullptr, 0, client_buffer);

    rocprofiler_callback_thread_t client_thread;
    rocprofiler_create_callback_thread(&client_thread);
    rocprofiler_assign_callback_thread(client_buffer, client_thread);

    int valid_ctx = 0;
    rocprofiler_context_is_valid(client_ctx, &valid_ctx);
    if (valid_ctx == 0)
    {
        return -1;
    }

    rocprofiler_start_context(client_ctx);

    return 0;
}

void tool_fini(void* tool_data)
{
    rocprofiler_flush_buffer(client_buffer);
}

extern "C" rocprofiler_tool_configure_result_t* rocprofiler_configure(uint32_t version,
                                                                      const char* runtime_version,
                                                                      uint32_t priority,
                                                                      rocprofiler_client_id_t* id)
{
    id->name = "lo2s";

    static rocprofiler_tool_configure_result_t cfg{ sizeof(rocprofiler_tool_configure_result_t),
                                                    &tool_init, &tool_fini, nullptr };
    return &cfg;
}
