/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/perf/tracepoint/recorder.hpp>

#include <lo2s/perf/tracepoint/event_format.hpp>
#include <lo2s/perf/tracepoint/writer.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/topology.hpp>

#include <nitro/lang/enumerate.hpp>

#include <boost/format.hpp>

extern "C" {
#include <poll.h>
}

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

recorder::recorder(trace::trace& trace_, const monitor_config& config,
                   const time::converter& time_converter)
{
    perf_recorders_.reserve(topology::instance().cpus().size() * config.tracepoint_events.size());
    // Note any of those setups might fail.
    // TODO: Currently is's all or nothing here, allow partial failure
    for (const auto& event_name : config.tracepoint_events)
    {
        event_format event(event_name);
        auto mc = trace_.metric_class();

        for (const auto& field : event.fields())
        {
            mc.add_member(trace_.metric_member(event_name + "::" + field.name(), "?",
                                               otf2::common::metric_mode::absolute_next,
                                               otf2::common::type::int64, "#"));
        }

        for (const auto& cpu : topology::instance().cpus())
        {
            log::debug() << "Create cstate recorder for cpu #" << cpu.id;
            perf_recorders_.emplace_back(cpu.id, event, config, trace_, mc, time_converter);
        }
    }
    start();
}

recorder::~recorder()
{
    // Thread MUST be stopped...
    stop();
    thread_.join();
}

void recorder::start()
{
    thread_ = std::thread([this]() { this->poll(); });
}

void recorder::poll()
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
                log::warn() << "Poll on raw event fds got unexpected event flags: " << pfd.revents
                            << ". Stopping raw event polling.";
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

void recorder::stop_all()
{
    for (auto& recorder : perf_recorders_)
    {
        recorder.stop();
    }
}

void recorder::stop()
{
    stop_pipe_.write();
}
}
}
}
