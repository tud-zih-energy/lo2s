#include <lo2s/tracepoint/otf2_tracepoints.hpp>
#include <lo2s/tracepoint/event_format.hpp>

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

otf2_tracepoints::otf2_tracepoints(otf2_trace& trace, const monitor_config& config,
                                   const perf_time_converter& time_converter)
{
    perf_recorders_.reserve(topology::instance().cpus().size() * config.tracepoint_events.size());
    // Note any of those setups might fail.
    // TODO: Currently is's all or nothing here, allow partial failure
    for (const auto& event_name : config.tracepoint_events)
    {
        event_format event(event_name);
        auto mc = trace.metric_class();

        for (const auto& field : event.fields())
        {
            mc.add_member(trace.metric_member(event_name + "::" + field.name(), "?",
                                              otf2::common::metric_mode::absolute_next,
                                              otf2::common::type::int64, "#"));
        }

        for (const auto& cpu : topology::instance().cpus())
        {
            log::debug() << "Create cstate recorder for cpu #" << cpu.id;
            perf_recorders_.emplace_back(cpu.id, event, config, trace, mc, time_converter);
        }
    }
    start();
}

otf2_tracepoints::~otf2_tracepoints()
{
    // Thread MUST be stopped...
    stop();
    thread_.join();
}

void otf2_tracepoints::start()
{
    thread_ = std::thread([this]() { this->poll(); });
}

void otf2_tracepoints::poll()
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

void otf2_tracepoints::stop_all()
{
    for (auto& recorder : perf_recorders_)
    {
        recorder.stop();
    }
}

void otf2_tracepoints::stop()
{
    stop_pipe_.write();
}
}
