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

#include <lo2s/metric_plugin.hpp>
#include <lo2s/metric_plugin_wrapper.hpp>
#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>

#include <memory>
#include <stdexcept>

namespace lo2s
{

static std::uint64_t get_time_wrapper()
{
    return get_time().time_since_epoch().count();
}

static std::string lib_name(const std::string& name)
{
    return std::string("lib") + name + ".so";
}

static std::string entry_name(const std::string& name)
{
    return std::string("SCOREP_MetricPlugin_") + name + "_get_info";
}

static void parse_properties(std::vector<metric_channel>& channels,
                             metric_plugin_wrapper::properties* props, otf2_trace& trace)
{
    if (props == nullptr)
    {
        throw std::runtime_error("Plugin returned a nullptr, where it shouldn't");
    }

    for (std::size_t index = 0; props[index].name != nullptr; index++)
    {
        // TODO Think about freeing name, description, unit
        //      It should be done, but Score-P doesn't do it either, even though it is documented.
        // std::unique_ptr<char, malloc_delete<char>> name_owner(props[index].name);
        // std::unique_ptr<char, malloc_delete<char>> desc_owner(props[index].description);
        // std::unique_ptr<char, malloc_delete<char>> unit_owner(props[index].unit);

        channels.emplace_back(props[index].name, props[index].description, props[index].unit,
                              props[index].mode, props[index].value_type, trace);
    }
}

metric_plugin::metric_plugin(const std::string& plugin_name,
                             const std::vector<std::string>& plugin_events, otf2_trace& trace)
: plugin_name_(plugin_name), plugin_events_(plugin_events), trace_(trace),
  lib_(lib_name(plugin_name)), plugin_()
{
    log::info() << "Loading plugin: " << plugin_name_;

    // open lib file and call entry point
    auto plugin_entry = lib_.load<metric_plugin_wrapper::plugin_info()>(entry_name(plugin_name_));
    plugin_ = plugin_entry();

    if (plugin_.sync != metric_plugin_wrapper::Synchronicity::ASYNC)
    {
        log::error() << "Plugin '" << plugin_name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are ASYNC are supported.");
    }

    if (plugin_.run_per != metric_plugin_wrapper::Per::ONCE &&
        plugin_.run_per != metric_plugin_wrapper::Per::HOST)
    {
        log::error() << "Plugin '" << plugin_name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are PER_HOST or ONCE are supported.");
    }

    plugin_.set_clock_function(&get_time_wrapper);
    plugin_.initialize();

    for (auto token : plugin_events_)
    {
        log::info() << "Plugin '" << plugin_name_ << "' calling get_event_info with: " << token;

        auto info = std::unique_ptr<metric_plugin_wrapper::properties,
                                    malloc_delete<metric_plugin_wrapper::properties>>(
            plugin_.get_event_info(token.c_str()));

        parse_properties(channels_, info.get(), trace);
    }

    log::info() << "Plugin '" << plugin_name_ << "' reported " << channels_.size() << " channels.";

    for (auto& channel : channels_)
    {
        auto id = plugin_.add_counter(channel.name().c_str());

        if (id == -1)
        {
            log::warn() << "Error in Plugin: " << plugin_name_
                        << " Failed to call add_counter for token: " << channel.name();

            continue;
        }

        channel.id() = id;
    }
}

metric_plugin::~metric_plugin()
{
    log::info() << "Unloading plugin: " << plugin_name_;
    plugin_.finalize();
}

void metric_plugin::fetch_data(otf2::chrono::time_point from, otf2::chrono::time_point to)
{
    for (auto& channel : channels_)
    {
        if (channel.id() == -1)
        {
            log::warn() << "In plugin: " << plugin_name_ << " skipping channel '" << channel.name()
                        << "' in data acquisition.";
            // something went wrong, skipping this channel
            continue;
        }

        metric_plugin_wrapper::time_value_pair* tv_list;
        auto num_entries = plugin_.get_all_values(channel.id(), &tv_list);
        std::unique_ptr<metric_plugin_wrapper::time_value_pair,
                        malloc_delete<metric_plugin_wrapper::time_value_pair>>
            tv_list_owner(tv_list);

        log::info() << "In plugin: " << plugin_name_ << " received for channel '" << channel.name()
                    << "' " << num_entries << " data points.";

        for (std::size_t i = 0; i < num_entries; ++i)
        {
            auto timestamp = otf2::chrono::time_point(otf2::chrono::duration(tv_list[i].timestamp));
            if (from <= timestamp && timestamp <= to)
            {
                channel.write_value(tv_list[i]);
            }
        }
    }
}

void metric_plugin::start_recording()
{
    log::info() << "Start recording for plugin: " << plugin_name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, metric_plugin_wrapper::SynchronizationMode::BEGIN);
    }
}
void metric_plugin::stop_recording()
{
    log::info() << "Stop recording for plugin: " << plugin_name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, metric_plugin_wrapper::SynchronizationMode::END);
    }
}
}
