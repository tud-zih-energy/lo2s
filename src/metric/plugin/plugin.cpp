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

#include <lo2s/metric/plugin/channel.hpp>
#include <lo2s/metric/plugin/plugin.hpp>
#include <lo2s/metric/plugin/wrapper.hpp>

#include <lo2s/log.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/chrono/duration.hpp>

#include <nitro/dl/dl.hpp>

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

static std::uint64_t lo2s_clock_function()
{
    return time::now().time_since_epoch().count();
}

static std::string lib_name(const std::string& name)
{
    return std::string("lib") + name + ".so";
}

static std::string entry_point_name(const std::string& name)
{
    return std::string("SCOREP_MetricPlugin_") + name + "_get_info";
}

Plugin::Plugin(const std::string& name, const std::vector<std::string>& events, trace::Trace& trace)
: name_(name), events_(events)
{
    load_plugin_code();
    assert_compatibility();
    initialize_plugin_code(trace);
}

Plugin::~Plugin()
{
    Log::info() << "Unloading plugin: " << name_;
    plugin_.finalize();
}

static auto read_data_points(const Channel& channel, const wrapper::PluginInfo& plugin)
{
    std::unique_ptr<wrapper::TimeValuePair[], wrapper::MallocDelete<wrapper::TimeValuePair>>
        tv_list;

    wrapper::TimeValuePair* ptr;
    auto num_entries = plugin.get_all_values(channel.id(), &ptr);
    tv_list.reset(ptr);

    return std::make_pair(num_entries, std::move(tv_list));
}

void Plugin::write_data_points(otf2::chrono::time_point from, otf2::chrono::time_point to)
{
    for (auto& channel : channels_)
    {
        if (channel.id() == -1)
        {
            Log::warn() << "In plugin: " << name_ << " skipping channel '" << channel.name()
                        << "' in data acquisition.";
            continue;
        }

        auto data = read_data_points(channel, plugin_);

        Log::info() << "In plugin: " << name_ << " received for channel '" << channel.name() << "' "
                    << data.first << " data points.";
        channel.write_values(data.second.get(), data.second.get() + data.first, from, to);
    }
}

void Plugin::start_recording()
{
    Log::debug() << "Start recording for plugin: " << name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, wrapper::SynchronizationMode::BEGIN);
    }
}

void Plugin::stop_recording()
{
    Log::debug() << "Stop recording for plugin: " << name_;

    if (plugin_.synchronize != nullptr)
    {
        plugin_.synchronize(true, wrapper::SynchronizationMode::END);
    }
}

void Plugin::assert_compatibility()
{
    if (plugin_.sync != wrapper::Synchronicity::ASYNC)
    {
        Log::error() << "Plugin '" << name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are ASYNC are supported.");
    }

    if (plugin_.run_per != wrapper::Per::ONCE && plugin_.run_per != wrapper::Per::HOST)
    {
        Log::error() << "Plugin '" << name_ << "' is incompatible.";
        throw std::runtime_error("Only plugins, which are PER_HOST or ONCE are supported.");
    }
}

void Plugin::load_plugin_code()
{
    Log::debug() << "Loading plugin: " << name_;

    nitro::dl::dl lib(lib_name(name_));
    auto entry_point = lib.load<wrapper::PluginInfo()>(entry_point_name(name_));
    plugin_ = entry_point();
}

void Plugin::initialize_plugin_code(trace::Trace& trace)
{
    plugin_.set_clock_function(&lo2s_clock_function);
    auto ret = plugin_.initialize();
    if (ret)
    {
        Log::error() << "Plugin '" << name_ << "' failed to initialize: " << ret;
        throw std::runtime_error("Plugin initialization failed.");
    }

    create_channels(trace);
    initialize_channels();
}

void Plugin::create_channels(trace::Trace& trace)
{
    for (const auto& token : events_)
    {
        Log::debug() << "Plugin '" << name_ << "' calling get_event_info with: " << token;

        auto info =
            std::unique_ptr<wrapper::Properties, wrapper::MallocDelete<wrapper::Properties>>(
                plugin_.get_event_info(token.c_str()));

        if (!info)
        {
            throw std::runtime_error("Plugin returned a nullptr, where it shouldn't");
        }

        for (std::size_t index = 0; info.get()[index].name != nullptr; index++)
        {
            const auto& event = info.get()[index];
            channels_.emplace_back(event.name, event.description, event.unit, event.mode,
                                   event.value_type, event.base, event.exponent, trace);
        }
    }
}

void Plugin::initialize_channels()
{
    auto log_info = Log::info() << "Plugin '" << name_ << "' recording channels: ";

    for (auto& channel : channels_)
    {
        auto id = plugin_.add_counter(channel.name().c_str());

        if (id == -1)
        {
            Log::warn() << "Error in Plugin: " << name_
                        << " Failed to call add_counter for token: " << channel.name();

            continue;
        }

        log_info << channel.name() << ", ";

        channel.id() = id;
    }
}
} // namespace plugin
} // namespace metric
} // namespace lo2s
