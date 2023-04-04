/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/nec/sensor_writer.cpp>

#include <filesystem>
#include <regex>

namespace lo2s
{
namespace nec
{

SensorWriter::SensorWriter(trace::Trace& trace)
{
    const std::regex device_regex(".*ve(\\d+)");
    std::smatch device_match;

    for (const auto& file : std::filesystem::directory_iterator("/sys/class/ve"))
    {
        if (std::regex_match(file.path(), device_match, device_regex))
        {
            devices_.emplace_back(Device(stoi(device_match[1].str())));
        }
    }
}
} // namespace nec
} // namespace lo2s
