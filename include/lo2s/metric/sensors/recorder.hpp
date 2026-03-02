// SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace lo2s::metric::sensors
{
class Recorder : public monitor::PollMonitor
{
public:
    Recorder(trace::Trace& trace);
    Recorder(Recorder&) = delete;
    Recorder(Recorder&&) = delete;

    Recorder& operator=(Recorder&) = delete;
    Recorder& operator=(Recorder&&) = delete;
    ~Recorder() override;

protected:
    void monitor(int fd) override;

    std::string group() const override
    {
        return "sensors::Monitor";
    }

private:
    otf2::writer::local& otf2_writer_;

    otf2::definition::metric_instance metric_instance_;
    std::unique_ptr<otf2::event::metric> event_;

    std::vector<std::pair<const void*, int>> items_;

    int fd_;
};
} // namespace lo2s::metric::sensors
