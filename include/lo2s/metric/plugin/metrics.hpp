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

#include <lo2s/trace/fwd.hpp>

#include <memory>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace plugin
{

class Plugin;

class Metrics
{
public:
    Metrics(trace::Trace& trace);

    ~Metrics();

    void start();
    void stop();

private:
    void fetch_plugins_data();
    void load_plugin(const std::string& name, const std::vector<std::string>& configuration);

private:
    trace::Trace& trace_;
    std::vector<std::unique_ptr<Plugin>> metric_plugins_;
    bool running_ = false;
};
} // namespace plugin
} // namespace metric
} // namespace lo2s
