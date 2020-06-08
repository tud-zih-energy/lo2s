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

#include <lo2s/metric/plugin/manager.hpp>

#include <lo2s/metric/plugin/configuration.hpp>
#include <lo2s/metric/plugin/plugin.hpp>

#include <lo2s/trace/trace.hpp>

#include <lo2s/util/env.hpp>

#include <nitro/dl/dl.hpp>
#include <nitro/lang/reverse.hpp>
#include <nitro/lang/string.hpp>

namespace lo2s
{
namespace metric
{
namespace plugin
{

static auto read_plugin_configurations()
{
    std::vector<Configuration> configs;

    for (const auto& plugin : nitro::lang::split(util::get_scorep_compatible_env_variable("METRIC_PLUGINS"), ","))
    {
        if (plugin.empty())
        {
            continue;
        }

        configs.emplace_back(plugin);
    }

    return configs;
}

PluginManager::PluginManager(trace::Trace& trace)
{
    for (const auto& config : read_plugin_configurations())
    {
        load_plugin(config, trace);
    }
}

void PluginManager::load_plugin(const Configuration& config, trace::Trace& trace)
{
    try
    {
        metric_plugins_.emplace_back(std::make_unique<Plugin>(config, trace));
    }
    catch (nitro::dl::exception& e)
    {
        Log::error() << "skipping plugin " << config.name() << ": " << e.what() << " ("
                     << e.dlerror() << ")";
    }
    catch (std::exception& e)
    {
        Log::error() << "skipping plugin " << config.name() << ": " << e.what();
    }
}

PluginManager::~PluginManager()
{
    if (running_)
    {
        stop_recording();
    }

    // this is only called in the dtor to ensure that trace_.record_to is valid
    write_metric_data();
}

void PluginManager::write_metric_data()
{
    assert(!running_);

    // Iterate in reverse order to keep interval semantic for plugin lifetimes
    for (auto& plugin : nitro::lang::reverse(metric_plugins_))
    {
        plugin->write_metrics();
    }
}

void PluginManager::start_recording()
{
    for (auto& plugin : metric_plugins_)
    {
        plugin->start_recording();
    }
    running_ = true;
}

void PluginManager::stop_recording()
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
