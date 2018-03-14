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

#include <nitro/env/get.hpp>

#include <algorithm>
#include <sstream>
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

static std::vector<std::string> explode(const std::string& str, char delim = ',')
{
    std::stringstream s(str);

    std::string line;

    std::vector<std::string> v;

    while (std::getline(s, line, delim))
    {
        v.push_back(line);
    }

    return v;
}

static auto upper_case(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);

    return input;
}

auto read_env(const std::string& name)
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

static auto read_env_variables()
{
    std::vector<std::pair<std::string, std::vector<std::string>>> v;

    for (const auto& plugin : explode(read_env("METRIC_PLUGINS")))
    {
        auto events = read_env(std::string("METRIC_") + upper_case(plugin));

        if (events.empty())
        {
            events = read_env(std::string("METRIC_") + upper_case(plugin) + "_PLUGIN");
        }

        v.push_back({ plugin, explode(events) });
    }

    return v;
}

Metrics::Metrics(trace::Trace& trace) : trace_(trace)
{
    // initialize each plugin, which is set using the SCOREP_METRIC_PLUGINS variables
    for (const auto& plugin_name_options : read_env_variables())
    {
        try
        {
            metric_plugins_.emplace_back(std::make_unique<Plugin>(
                plugin_name_options.first, plugin_name_options.second, trace_));
        }
        catch (std::exception& e)
        {
            Log::error() << "skipping plugin " << plugin_name_options.first << ": " << e.what();
        }
    }

    // Start recording for each plugin
    for (auto& plugin : metric_plugins_)
    {
        plugin->start_recording();
    }
}

Metrics::~Metrics()
{
    // first we stop recording, after that, we collect the data from each plugin
    // and the destructor of the vector member will finalize each plugin

    // In order to get interval semantic for plugin lifetimes, we have to iterate in reverse order
    for (auto pi = metric_plugins_.rbegin(); pi != metric_plugins_.rend(); ++pi)
    {
        (*pi)->stop_recording();
    }

    // NOTE: These are on purpose two seperate iterations
    for (auto pi = metric_plugins_.rbegin(); pi != metric_plugins_.rend(); ++pi)
    {
        // in order to not create to large traces (in the sense of to much time before and after
        // the actuall recorded program(s)), we stick to the interval [trace_begin_, trace_end]
        (*pi)->fetch_data(trace_.record_from(), trace_.record_to());
    }
}
} // namespace plugin
} // namespace metric
} // namespace lo2s
