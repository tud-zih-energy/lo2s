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

#include <lo2s/metric/plugin/metrics.hpp>
#include <lo2s/metric/plugin/plugin.hpp>

#include <lo2s/trace/trace.hpp>

#include <nitro/dl/dl.hpp>
#include <nitro/env/get.hpp>
#include <nitro/lang/reverse.hpp>
#include <nitro/lang/string.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <cctype>

namespace lo2s
{
namespace metric
{
namespace plugin
{

static auto upper_case(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);

    return input;
}

static auto read_env_variable(const std::string& name)
{
    try
    {
        return nitro::env::get(std::string("LO2S_") + name, nitro::env::no_default);
    }
    catch (std::exception& e)
    {
        return nitro::env::get(std::string("SCOREP_") + name);
    }
}

static auto read_plugin_configuration()
{
    std::vector<std::pair<std::string, std::vector<std::string>>> v;

    for (const auto& plugin : nitro::lang::split(read_env_variable("METRIC_PLUGINS"), ","))
    {
        if (plugin.empty())
        {
            continue;
        }

        auto events = read_env_variable(std::string("METRIC_") + upper_case(plugin));

        if (events.empty())
        {
            events = read_env_variable(std::string("METRIC_") + upper_case(plugin) + "_PLUGIN");
        }

        v.push_back({ plugin, nitro::lang::split(events, ",") });
    }

    return v;
}

Metrics::Metrics(trace::Trace& trace) : trace_(trace)
{
    for (const auto& plugin : read_plugin_configuration())
    {
        load_plugin(plugin.first, plugin.second);
    }
}

void Metrics::load_plugin(const std::string& name, const std::vector<std::string>& configuration)
{
    try
    {
        metric_plugins_.emplace_back(std::make_unique<Plugin>(name, configuration, trace_));
    }
    catch (nitro::dl::exception& e)
    {
        Log::error() << "skipping plugin " << name << ": " << e.what() << " (" << e.dlerror()
                     << ")";
    }
    catch (std::exception& e)
    {
        Log::error() << "skipping plugin " << name << ": " << e.what();
    }
}

Metrics::~Metrics()
{
    if (running_)
    {
        stop();
    }

    // this is only called in the dtor to ensure that trace_.record_to is valid
    fetch_plugins_data();
}

void Metrics::fetch_plugins_data()
{
    assert(!running_);

    // Iterate in reverse order to keep interval semantic for plugin lifetimes
    for (auto& plugin : nitro::lang::reverse(metric_plugins_))
    {
        plugin->write_data_points(trace_.record_from(), trace_.record_to());
    }
}

void Metrics::start()
{
    for (auto& plugin : metric_plugins_)
    {
        plugin->start_recording();
    }
    running_ = true;
}

void Metrics::stop()
{
    // In order to get interval semantic for plugin lifetimes, we have to iterate in reverse order
    for (auto& plugin : nitro::lang::reverse(metric_plugins_))
    {
        plugin->stop_recording();
    }
    running_ = false;
}
} // namespace plugin
} // namespace metric
} // namespace lo2s
