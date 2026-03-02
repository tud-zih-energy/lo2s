// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/metric/plugin/metrics.hpp>

#include <lo2s/log.hpp>
#include <lo2s/metric/plugin/plugin.hpp>
#include <lo2s/trace/trace.hpp>

#include <nitro/dl/exception.hpp>
#include <nitro/env/get.hpp>
#include <nitro/lang/string.hpp>

#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <cctype>

namespace lo2s::metric::plugin
{

namespace
{

auto upper_case(std::string input)
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

auto read_env_variables()
{
    std::vector<std::pair<std::string, std::vector<std::string>>> v;

    for (const auto& plugin : nitro::lang::split(read_env("METRIC_PLUGINS"), ","))
    {
        if (plugin.empty())
        {
            continue;
        }

        auto events = read_env(std::string("METRIC_") + upper_case(plugin));

        if (events.empty())
        {
            events = read_env(std::string("METRIC_") + upper_case(plugin) + "_PLUGIN");
        }

        v.push_back({ plugin, nitro::lang::split(events, ",") }); // NOLINT
    }

    return v;
}

} // namespace

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
        catch (nitro::dl::exception& e)
        {
            Log::error() << "skipping plugin " << plugin_name_options.first << ": " << e.what()
                         << " (" << e.dlerror() << ")";
        }
        catch (std::exception& e)
        {
            Log::error() << "skipping plugin " << plugin_name_options.first << ": " << e.what();
        }
    }
}

Metrics::~Metrics()
{
    // We do the fetch in the dtor to ensure that trace_.record_to is valid

    // first we stop recording, after that, we collect the data from each plugin
    // and the destructor of the vector member will finalize each plugin

    if (running_)
    {
        stop();
    }

    // In order to get interval semantic for plugin lifetimes, we have to iterate in reverse order
    for (auto pi = metric_plugins_.rbegin(); pi != metric_plugins_.rend(); ++pi)
    {
        // in order to not create to large traces (in the sense of to much time before and after
        // the actuall recorded program(s)), we stick to the interval [trace_begin_, trace_end]
        (*pi)->fetch_data(trace_.record_from(), trace_.record_to());
    }
}

void Metrics::start()
{
    // Start recording for each plugin
    for (auto& plugin : metric_plugins_)
    {
        plugin->start_recording();
    }
    running_ = true;
}

void Metrics::stop()
{
    // In order to get interval semantic for plugin lifetimes, we have to iterate in reverse order
    for (auto pi = metric_plugins_.rbegin(); pi != metric_plugins_.rend(); ++pi)
    {
        (*pi)->stop_recording();
    }
    running_ = false;
}
} // namespace lo2s::metric::plugin
