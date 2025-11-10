#pragma once

#include <lo2s/ompt/events.hpp>
#include <lo2s/ringbuf.hpp>

namespace lo2s
{
namespace omp
{

class RingbufWriter : public lo2s::RingbufWriter
{
public:
    RingbufWriter(Process process) : lo2s::RingbufWriter(process, RingbufMeasurementType::OPENMP)
    {
    }

    bool ompt_enter(uint64_t tp, OMPTCctx cctx)
    {

        if (finalized())
        {
            return false;
        }
        std::lock_guard<std::mutex> guard(mutex_);

        struct ompt_enter* ev = reserve<struct ompt_enter>();

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
        if (finalized())
        {
            return false;
        }

        std::lock_guard<std::mutex> guard(mutex_);

        struct ompt_exit* ev = reserve<struct ompt_exit>();

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

} // namespace omp
} // namespace lo2s
