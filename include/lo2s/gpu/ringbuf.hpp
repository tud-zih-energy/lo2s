#pragma once

#include <lo2s/gpu/events.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/rb/writer.hpp>
#include <lo2s/types/process.hpp>

#include <map>
#include <string>

#include <cstdint>
#include <cstring>

namespace lo2s::gpu
{

class RingbufWriter : public lo2s::RingbufWriter
{
public:
    RingbufWriter(Process process) : lo2s::RingbufWriter(process, RingbufMeasurementType::GPU)
    {
    }

    // Creates new local cctx_ref. Returns result from local map if it already
    // exists. Otherwise, generates a kernel_def event.
    uint64_t kernel_def(std::string func)
    {
        if (cctxs_.count(func))
        {
            return cctxs_.at(func);
        }

        auto* ev = reserve<struct kernel_def>(func.size());

        if (ev == nullptr)
        {
            return 0;
        }

        auto cctx = cctxs_.emplace(func, next_cctx_ref_);

        memcpy(ev->function, func.c_str(), func.size());

        ev->header.type = (uint64_t)EventType::KERNEL_DEF;
        ev->kernel_id = cctx.first->second;

        next_cctx_ref_++;

        commit();
        return cctx.second;
    }

    bool kernel_def(const std::string& func, uint64_t kernel_id)
    {
        auto* ev = reserve<struct kernel_def>(func.size());

        if (ev == nullptr)
        {
            return false;
        }

        memcpy(ev->function, func.c_str(), func.size());

        ev->header.type = (uint64_t)EventType::KERNEL_DEF;
        ev->kernel_id = kernel_id;

        commit();
        return true;
    }

    bool kernel(uint64_t start_tp, uint64_t end_tp, uint64_t kernel_id)
    {
        auto* ev = reserve<struct kernel>();

        if (ev == nullptr)
        {
            return false;
        }
        ev->header.type = (uint64_t)EventType::KERNEL;

        ev->start_tp = start_tp;
        ev->end_tp = end_tp;
        ev->kernel_id = kernel_id;

        commit();
        return true;
    }

private:
    std::map<std::string, uint64_t> cctxs_;
    uint64_t next_cctx_ref_ = 0;
};

} // namespace lo2s::gpu
