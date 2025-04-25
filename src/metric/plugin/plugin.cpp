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

#include <lo2s/metric/plugin/plugin.hpp>

#include <lo2s/log.hpp>
#include <lo2s/metric/plugin/channel.hpp>
#include <lo2s/metric/plugin/wrapper.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/util.hpp>

#include <otf2xx/chrono/duration.hpp>

#include <memory>
#include <stdexcept>

#include <cstddef>
#include <cstdint>

namespace lo2s
{

namespace metric
{
namespace plugin
{

static std::uint64_t get_time_wrapper()
{
    return time::now().time_since_epoch().count();
}

static std::string lib_name(const std::string& name)
{
    return std::string("lib") + name + ".so";
}

static std::string entry_name(const std::string& name)
{
    return std::string("SCOREP_MetricPlugin_") + name + "_get_info";
}

static void parse_properties(std::vector<Channel>& channels, wrapper::Properties* props,
                             trace::Trace& trace)
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
                              props[index].mode, props[index].value_type, props[index].base,
                              props[index].exponent, trace);
    }
}

Plugin::Plugin(const std::string& plugin_name, const std::vector<std::string>& plugin_events,
               trace::Trace& trace)
: plugin_name_(plugin_name), plugin_events_(plugin_events), lib_(lib_name(plugin_name)), plugin_()
{
    Log::debug() << "Loading plugin: " << plugin_name_;

    // open lib file and call entry point
    auto plugin_entry = lib_.load<wrapper::PluginInfo()>(entry_name(plugin_name_));
    plugin_ = plugin_entry();

    if (plugin_.sync != wrapper::Synchronicity::ASYNC)
    {
        Log::error() << "Plugin '" << plugin_name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are ASYNC are supported.");
    }

    if (plugin_.run_per != wrapper::Per::ONCE && plugin_.run_per != wrapper::Per::HOST)
    {
        Log::error() << "Plugin '" << plugin_name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are PER_HOST or ONCE are supported.");
    }

    plugin_.set_clock_function(&get_time_wrapper);
    auto ret = plugin_.initialize();
    if (ret)
    {
        Log::error() << "Plugin '" << plugin_name_ << "' failed to initialize: " << ret;
        throw std::runtime_error("Plugin initialization failed.");
    }

    for (auto token : plugin_events_)
    {
        Log::debug() << "Plugin '" << plugin_name_ << "' calling get_event_info with: " << token;

        auto info = std::unique_ptr<wrapper::Properties, memory::MallocDelete<wrapper::Properties>>(
            plugin_.get_event_info(token.c_str()));

        parse_properties(channels_, info.get(), trace);
    }

    auto log_info = Log::info() << "Plugin '" << plugin_name_ << "' recording channels: ";

    for (auto& channel : channels_)
    {
        auto id = plugin_.add_counter(channel.name().c_str());

        if (id == -1)
        {
            Log::warn() << "Error in Plugin: " << plugin_name_
                        << " Failed to call add_counter for token: " << channel.name();

            continue;
        }

        log_info << channel.name() << ", ";

        channel.id() = id;
    }
}

Plugin::~Plugin()
{
    Log::info() << "Unloading plugin: " << plugin_name_;
    plugin_.finalize();
}

void Plugin::fetch_data(otf2::chrono::time_point from, otf2::chrono::time_point to)
{
    for (auto& channel : channels_)
    {
        if (channel.id() == -1)
        {
            Log::warn() << "In plugin: " << plugin_name_ << " skipping channel '" << channel.name()
                        << "' in data acquisition.";
            // something went wrong, skipping this channel
            continue;
        }

        wrapper::TimeValuePair* tv_list;
        auto num_entries = plugin_.get_all_values(channel.id(), &tv_list);
        std::unique_ptr<wrapper::TimeValuePair, memory::MallocDelete<wrapper::TimeValuePair>>
            tv_list_owner(tv_list);

        Log::info() << "In plugin: " << plugin_name_ << " received for channel '" << channel.name()
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

void Plugin::start_recording()
{
    Log::debug() << "Start recording for plugin: " << plugin_name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, wrapper::SynchronizationMode::BEGIN);
    }
}

void Plugin::stop_recording()
{
    Log::debug() << "Stop recording for plugin: " << plugin_name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, wrapper::SynchronizationMode::END);
    }
}
} // namespace plugin
} // namespace metric
} // namespace lo2s
