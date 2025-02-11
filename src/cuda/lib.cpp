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

#include <lo2s/cuda/ringbuf.hpp>

#include <iostream>
#include <memory>
#include <string>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C"
{
#include <cupti.h>
#include <time.h>
#include <unistd.h>
}

// Allocate 8 MiB every time CUPTI asks for more event memory
constexpr size_t CUPTI_BUFFER_SIZE = 8 * 1024 * 1024;

std::unique_ptr<lo2s::cuda::RingbufWriter> rb_writer = nullptr;

CUpti_SubscriberHandle subscriber = nullptr;

static void atExitHandler(void)
{
    // Flush all remaining activity records
    cuptiActivityFlushAll(1);
}

// Through bufferRequested, CUPTI asks for more memory to fill with events
static void CUPTIAPI bufferRequested(uint8_t** buffer, size_t* size, size_t* maxNumRecords)
{
    assert(buffer != nullptr && size != nullptr && maxNumRecords != nullptr);

    *maxNumRecords = 0;
    *size = CUPTI_BUFFER_SIZE;
    *buffer = static_cast<uint8_t*>(malloc(*size));

    if (*buffer == nullptr)
    {
        std::cerr << "Error: Out of memory.\n";
        exit(-1);
    }
}

// bufferCompleted is called when a requested buffer (created through bufferRequested) has
//  been filled with event data or the end of measurement is reached. We then can process the events
//  in that CUPTI event buffer and write them to the lo2s ring-buffer
static void CUPTIAPI bufferCompleted(CUcontext ctx, uint32_t streamId, uint8_t* buffer, size_t size,
                                     size_t validSize)
{
    CUpti_Activity* record = nullptr;

    size_t ringbuf_full_dropped = 0;
    while (cuptiActivityGetNextRecord(buffer, validSize, &record) == CUPTI_SUCCESS)
    {
        switch (record->kind)
        {
        case CUPTI_ACTIVITY_KIND_KERNEL:
        case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
        {
            CUpti_ActivityKernel6* kernel = reinterpret_cast<CUpti_ActivityKernel6*>(record);

            std::string kernel_name = kernel->name;
            uint64_t cctx = 0;
            cctx = rb_writer->kernel_def(kernel_name);

            rb_writer->kernel(kernel->start, kernel->end, cctx);
            break;
        }
        default:
            break;
        }
    }

    size_t cupti_dropped;
    cuptiActivityGetNumDroppedRecords(ctx, streamId, &cupti_dropped);
    if (cupti_dropped != 0)
    {
        std::cerr << "Dropped " << cupti_dropped << " activity records in CUPTI.\n";
    }

    if (ringbuf_full_dropped != 0)
    {
        std::cerr << "lo2s Ringbuffer full, dropped" << ringbuf_full_dropped
                  << " events. Try to increase --nvidia-ringbuf-size!"

                  << std::endl;
    }

    free(buffer);
}

// callbackHandler is our universal callback handler for the callback based part of the CUPTI
//  event API. We attach it to the following events:
//
//  - cuProfilerStart -> enable the CUPTI Activity API tracing for the given cuContext
//  - cuProfilerStop -> flush event buffers and disable CUPTI Activity API tracing
//  - cudaDeviceReset -> flush event buffers
void CUPTIAPI callbackHandler(void* userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid,
                              void* cbdata)
{
    const CUpti_CallbackData* cbInfo = (CUpti_CallbackData*)cbdata;

    if (domain == CUPTI_CB_DOMAIN_DRIVER_API)
    {
        if (cbid == CUPTI_DRIVER_TRACE_CBID_cuProfilerStart)
        {
            if (cbInfo->callbackSite == CUPTI_API_EXIT)
            {
                cuptiActivityEnableContext(cbInfo->context, CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
            }
        }
        else if (cbid == CUPTI_DRIVER_TRACE_CBID_cuProfilerStop)
        {
            if (cbInfo->callbackSite == CUPTI_API_ENTER)
            {
                cuptiActivityFlushAll(0);
                cuptiEnableCallback(0, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API,
                                    CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020);

                cuptiActivityDisableContext(cbInfo->context, CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
            }
        }
    }

    // Also flush on CUDA device reset
    else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API)
    {
        if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020)
        {
            if (cbInfo->callbackSite == CUPTI_API_ENTER)
            {
                cuptiActivityFlushAll(0);
            }
        }
    }
}

uint64_t timestampfunc()
{
    return rb_writer->timestamp();
}

extern "C" int InitializeInjection(void)
{
    pid_t pid = getpid();

    rb_writer = std::make_unique<lo2s::cuda::RingbufWriter>(lo2s::Process(pid));

    // Register an atexit() handler for clean-up
    atexit(&atExitHandler);

    cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)callbackHandler, nullptr);

    // Supply our own timestamp generation function. Saves us the work of converting timestamps
    cuptiActivityRegisterTimestampCallback(timestampfunc);

    // Register CUDA API callbacks for us to attach to new CUDA contexts
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStart);
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStop);

    cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);

    // Register buffer callbacks. When cupti needs a new buffer for recording date, it calls
    // bufferRequested. When the buffer is full, bufferCompleted is used to write the data to the
    // lo2s ring-buffer
    cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted);

    return 1;
}
