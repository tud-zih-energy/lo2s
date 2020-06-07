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

#pragma once

#include <lo2s/metric/plugin/channel.hpp>
#include <lo2s/metric/plugin/configuration.hpp>
#include <lo2s/metric/plugin/wrapper.hpp>

#include <lo2s/trace/fwd.hpp>

#include <otf2xx/chrono/time_point.hpp>

#include <string>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace plugin
{
class Plugin
{
public:
    Plugin(const Configuration& config, trace::Trace& trace);

    ~Plugin();

    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    void start_recording();
    void stop_recording();
    void write_data_points(otf2::chrono::time_point from, otf2::chrono::time_point to);

    const std::string name() const
    {
        return config_.name();
    }

private:
    void load_plugin_code();
    void assert_compatibility();
    void initialize_plugin_code(trace::Trace& trace);
    void create_channels(trace::Trace& trace);
    void initialize_channels();

private:
    Configuration config_;
    wrapper::PluginInfo plugin_;
    std::vector<Channel> channels_;
};
} // namespace plugin
} // namespace metric
} // namespace lo2s
