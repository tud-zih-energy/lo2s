// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/metric/plugin/channel.hpp>
#include <lo2s/metric/plugin/wrapper.hpp>
#include <lo2s/trace/fwd.hpp>

#include <nitro/dl/dl.hpp>
#include <otf2xx/chrono/time_point.hpp>

#include <string>
#include <vector>

namespace lo2s::metric::plugin
{
class Plugin
{
public:
    Plugin(const std::string& plugin_name, const std::vector<std::string>& plugin_events,
           trace::Trace& trace);

    ~Plugin();

    Plugin(const Plugin&) = delete;
    Plugin(Plugin&&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin& operator=(const Plugin&&) = delete;

    void start_recording();

    void stop_recording();

    void fetch_data(otf2::chrono::time_point from, otf2::chrono::time_point to);

    std::string name() const
    {
        return plugin_name_;
    }

private:
    std::string plugin_name_;
    std::vector<std::string> plugin_events_;
    nitro::dl::dl lib_;
    wrapper::PluginInfo plugin_;
    std::vector<Channel> channels_;
};
} // namespace lo2s::metric::plugin
