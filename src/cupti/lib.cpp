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

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <lo2s/cupti/events.hpp>
#include <lo2s/ringbuf.hpp>
#include <string>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include <cupti.h>
extern "C"
{
#include <time.h>
#include <unistd.h>
}

// Allocate 8 MiB every time CUPTI asks for more event memory
constexpr size_t CUPTI_BUFFER_SIZE = 8 * 1024 * 1024;

std::unique_ptr<lo2s::RingBufWriter> rb_writer;
CUpti_SubscriberHandle subscriber = nullptr;

clockid_t clockid = CLOCK_MONOTONIC_RAW;

static void atExitHandler(void)
{
    // Flush all remaining activity records
    cuptiActivityFlushAll(1);
}

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

static void CUPTIAPI bufferCompleted(CUcontext ctx, uint32_t streamId, uint8_t* buffer, size_t size,
                                     size_t validSize)
{
    CUpti_Activity* record = nullptr;
    while (cuptiActivityGetNextRecord(buffer, validSize, &record) == CUPTI_SUCCESS)
    {
        switch (record->kind)
        {
        case CUPTI_ACTIVITY_KIND_KERNEL:
        case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
        {
            CUpti_ActivityKernel6* kernel = (CUpti_ActivityKernel6*)record;

            uint64_t name_len = strlen(kernel->name);

            struct lo2s::cupti::event_kernel* ev =
                (struct lo2s::cupti::event_kernel*)rb_writer->reserve(
                    sizeof(struct lo2s::cupti::event_kernel) + name_len);

            if (ev == nullptr)
            {
                // std::cerr << "Ringbuffer full, dropping event. Try to increase
                // --nvidia-ringbuf-size!" << std::endl;
                continue;
            }

            while (1)
            {
                std::cerr << "LIB: " << kernel->start << kernel->end << ":" << ev << std::endl;
            }
            ev->header.type = lo2s::cupti::EventType::CUPTI_KERNEL;
            ev->header.size = sizeof(struct lo2s::cupti::event_kernel) + name_len;
            ev->start = kernel->start;
            ev->end = kernel->end;
            std::cerr << kernel->name << std::endl;
            memcpy(ev->name, kernel->name, name_len + 1);

            rb_writer->commit();
            break;
        }
        default:
            break;
        }
    }

    size_t dropped;
    cuptiActivityGetNumDroppedRecords(ctx, streamId, &dropped);
    if (dropped != 0)
    {
        std::cerr << "Dropped " << dropped << " activity records.\n";
    }

    free(buffer);
}

static CUptiResult enableCuptiActivity(CUcontext ctx)
{
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API,
                        CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020);

    if (ctx == nullptr)
    {
        return cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
    }
    else
    {
        return cuptiActivityEnableContext(ctx, CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
    }
}

void CUPTIAPI callbackHandler(void* userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid,
                              void* cbdata)
{
    const CUpti_CallbackData* cbInfo = (CUpti_CallbackData*)cbdata;

    if (domain == CUPTI_CB_DOMAIN_DRIVER_API)
    {
        if (cbid == CUPTI_DRIVER_TRACE_CBID_cuProfilerStart)
        {
            /* We start profiling collection on exit of the API. */
            if (cbInfo->callbackSite == CUPTI_API_EXIT)
            {
                enableCuptiActivity(cbInfo->context);
            }
        }
        else if (cbid == CUPTI_DRIVER_TRACE_CBID_cuProfilerStop)
        {
            /* We stop profiling collection on entry of the API. */
            if (cbInfo->callbackSite == CUPTI_API_ENTER)
            {
                cuptiActivityFlushAll(0);
                cuptiEnableCallback(0, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API,
                                    CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020);

                cuptiActivityDisableContext(cbInfo->context, CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
            }
        }
    }
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
    struct timespec ts;
    clock_gettime(clockid, &ts);
    uint64_t res = ts.tv_sec * 1000000000 + ts.tv_nsec;
    return res;
}

extern "C" int InitializeInjection(void)
{
    std::string rb_size_str;
    rb_writer = std::make_unique<lo2s::RingBufWriter>(std::to_string(getpid()), false);

    char* clockid_str = getenv("LO2S_CLOCKID");

    if (clockid_str != nullptr)
    {
        clockid = std::stoi(clockid_str);
    }

    atexit(&atExitHandler);

    cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)callbackHandler, nullptr);

    cuptiActivityRegisterTimestampCallback(timestampfunc);

    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStart);
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStop);
    enableCuptiActivity(nullptr);

    // Register buffer callbacks
    cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted);

    return 1;
}
