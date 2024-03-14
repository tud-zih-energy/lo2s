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
#include <lo2s/cupti/ringbuf.hpp>

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

std::unique_ptr<RingBufWriter> rb_writer;
CUpti_SubscriberHandle subscriber = NULL;

static void atExitHandler(void)
{
    cuptiActivityFlushAll(1);
}

static void CUPTIAPI bufferRequested(uint8_t** buffer, size_t* size, size_t* maxNumRecords)
{
    *size = 8 * 1024 * 1024;
    *buffer = (uint8_t*)malloc(*size);
    *maxNumRecords = 0;

    if (*buffer == NULL)
    {
        std::cerr << "Error: Out of memory.\n";
        exit(-1);
    }
}

static void CUPTIAPI bufferCompleted(CUcontext ctx, uint32_t streamId, uint8_t* buffer, size_t size,
                                     size_t validSize)
{
    CUpti_Activity* record = NULL;
    while (cuptiActivityGetNextRecord(buffer, validSize, &record) == CUPTI_SUCCESS)
    {
        switch (record->kind)
        {
        case CUPTI_ACTIVITY_KIND_KERNEL:
        case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
        {
            const char* kindString =
                (record->kind == CUPTI_ACTIVITY_KIND_KERNEL) ? "KERNEL" : "CONC KERNEL";
            CUpti_ActivityKernel6* kernel = (CUpti_ActivityKernel6*)record;

            uint64_t len = sizeof(struct cupti_event_kernel) + strlen(kernel->name);
            struct cupti_event_kernel* ev = (struct cupti_event_kernel*)rb_writer->reserve(
                sizeof(struct cupti_event_kernel) + strlen(kernel->name));

            ev->header.type = CuptiEventType::CUPTI_KERNEL;
            ev->header.size = len;
            ev->start = kernel->start;
            ev->end = kernel->end;
            memcpy(ev->name, kernel->name, strlen(kernel->name) + 1);

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
        std::cerr << "Dropped %u activity records.\n", (unsigned int)dropped;
    }

    free(buffer);
}

static CUptiResult enableCuptiActivity(CUcontext ctx)
{
    auto result = CUPTI_SUCCESS;
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API,
                        CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020);

    if (ctx == NULL)
    {
        result = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
    }
    else
    {
        result = cuptiActivityEnableContext(ctx, CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
    }
    return result;
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
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t res = ts.tv_sec * 1000000000 + ts.tv_nsec;
    return res;
}

extern "C" int InitializeInjection(void)
{
    std::string rb_size_str;
    try
    {
        rb_size_str = getenv("LO2S_RINGBUF_SIZE");
        uint64_t rb_size = stoi(rb_size_str);
        rb_writer = std::make_unique<RingBufWriter>(std::to_string(getpid()), rb_size);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Could not read lo2s ringbuffer size!";
        exit(EXIT_FAILURE);
    }

    atexit(&atExitHandler);

    cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)callbackHandler, NULL);

    cuptiActivityRegisterTimestampCallback(timestampfunc);

    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStart);
    cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API,
                        CUPTI_DRIVER_TRACE_CBID_cuProfilerStop);
    enableCuptiActivity(NULL);

    // Register buffer callbacks
    cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted);

    return 1;
}
