#pragma once

#include <lo2s/ompt/events.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/rb/writer.hpp>
#include <lo2s/types/process.hpp>

#include <mutex>

#include <cstdint>

namespace lo2s::ompt
{
class RingbufWriter : public lo2s::RingbufWriter
{
public:
    RingbufWriter(Process process) : lo2s::RingbufWriter(process, RingbufMeasurementType::OPENMP)
    {
    }

    bool ompt_enter(uint64_t tp, OMPTCctx cctx)
    {
        const std::lock_guard<std::mutex> guard(mutex_);

        auto* ev = reserve<struct ompt_enter>();

        if (ev == nullptr)
        {
            return false;
        }

        ev->tp = tp;
        ev->header.type = (uint64_t)EventType::OMPT_ENTER;
        ev->cctx = cctx;

        commit();

        return true;
    }

    bool ompt_leave(uint64_t tp, OMPTCctx cctx)
    {
        std::lock_guard<std::mutex> const guard(mutex_);

        auto* ev = reserve<struct ompt_exit>();

        if (ev == nullptr)
        {
            return false;
        }

        ev->header.type = (uint64_t)EventType::OMPT_EXIT;
        ev->tp = tp;
        ev->cctx = cctx;

        commit();

        return true;
    }

private:
    std::mutex mutex_;
};

} // namespace lo2s::ompt
