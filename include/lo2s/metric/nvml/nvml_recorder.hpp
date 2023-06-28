/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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

#pragma once

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>

extern "C"
{
#include <nvml.h>
}

namespace lo2s
{
namespace metric
{
namespace nvml
{
class Recorder : public monitor::PollMonitor
{
public:
    Recorder(trace::Trace& trace);
    ~Recorder();

protected:
    void monitor(int fd) override;

    std::string group() const override
    {
        return "nvml::Monitor";
    }

private:
    otf2::writer::local& otf2_writer_;

    otf2::definition::metric_instance metric_instance_;
    std::unique_ptr<otf2::event::metric> event_;

    nvmlReturn_t result;
    nvmlDevice_t device;

};
} // namespace nvml
} // namespace metric
} // namespace lo2s
