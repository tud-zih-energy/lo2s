#ifndef LO2S_OTF2_COUNTERS_RAW_HPP
#define LO2S_OTF2_COUNTERS_RAW_HPP

#include "perf_sample_raw_otf2.hpp"

#include "../error.hpp"
#include "../log.hpp"
#include "../monitor_config.hpp"
#include "../otf2_trace.hpp"
#include "../pipe.hpp"
#include "../time.hpp"
#include "../topology.hpp"

#include <nitro/lang/enumerate.hpp>

#include <boost/format.hpp>

extern "C" {
#include <poll.h>
}

namespace lo2s
{
/*
 * NOTE: Encapsulates counters for ALL cpus, totally different than otf2_counters!
 */
class otf2_counters_raw
{
public:
    otf2_counters_raw(otf2_trace& trace, const monitor_config& config,
                      const perf_time_converter& time_converter)
    {
        auto mc = trace.metric_class();
        mc.add_member(trace.metric_member("cstate", "C state",
                                          otf2::common::metric_mode::absolute_next,
                                          otf2::common::type::int64, "state"));

        perf_recorders_.reserve(topology::instance().cpus().size());
        for (const auto& cpu : topology::instance().cpus())
        {
            log::debug() << "Create cstate recorder for cpu #" << cpu.id;
            perf_recorders_.emplace_back(cpu.id, config, trace, mc, time_converter);
        }
        start();
    }

    ~otf2_counters_raw()
    {
        // Thread MUST be stopped...
        stop();
        thread_.join();
    }

private:
    void start()
    {
        thread_ = std::thread([this]() { this->poll(); });
    }

    void poll()
    {
        std::vector<struct pollfd> pfds;
        pfds.reserve(perf_recorders_.size());
        for (const auto& rec : perf_recorders_)
        {
            struct pollfd pfd;
            pfd.fd = rec.fd();
            pfd.events = POLLIN;
            pfd.revents = 0;
            pfds.push_back(pfd);
        }
        {
            struct pollfd stop_pfd;
            stop_pfd.fd = stop_pipe_.read_fd();
            stop_pfd.events = POLLIN;
            stop_pfd.revents = 0;
            pfds.push_back(stop_pfd);
        }

        while (true)
        {
            auto ret = ::poll(pfds.data(), pfds.size(), -1);
            if (ret == 0)
            {
                throw std::runtime_error("Received poll timeout despite requesting no timeout.");
            }
            else if (ret < 0)
            {
                log::error() << "poll failed";
                throw_errno();
            }

            bool panic = false;
            for (const auto& pfd : pfds)
            {
                if (pfd.revents != 0 && pfd.revents != POLLIN)
                {
                    log::warn() << "Poll on raw event fds got unexpected event flags: "
                                << pfd.revents << ". Stopping raw event polling.";
                    panic = true;
                }
            }
            if (panic)
            {
                break;
            }

            if (pfds.back().revents & POLLIN)
            {
                log::debug() << "Requested stop of raw counters.";
                break;
            }

            log::trace() << "waking up for " << ret << " raw events.";
            for (const auto& index_recorder : nitro::lang::enumerate(perf_recorders_))
            {
                if (pfds[index_recorder.index()].revents & POLLIN)
                {
                    index_recorder.value().read();
                }
            }
        }
        stop_all();
        perf_recorders_.clear();
    }

    void stop_all()
    {
        for (auto& recorder : perf_recorders_)
        {
            recorder.stop();
        }
    }

public:
    void stop()
    {
        stop_pipe_.write();
    }

private:
    pipe stop_pipe_;
    std::vector<perf_sample_raw_otf2> perf_recorders_;
    std::thread thread_;
};
}

#endif // LO2S_OTF2_COUNTERS_RAW_HPP
