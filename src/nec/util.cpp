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

#include <lo2s/nec/util.hpp>

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace lo2s
{
namespace nec
{

uint64_t readSensor(int device_id, std::string sensor_name, bool is_hex)
{
    std::fstream sensor_file(fmt::format("/sys/class/ve{}/{}", device_id, sensor_name));
    uint64_t res;
    if (is_hex)
    {
        sensor_file >> std::hex >> res;
    }
    else
    {
        sensor_file >> res;
    }

    return res;
}

int numaCount(int device_id)
{
    if (!readSensor(device_id, "partitioning_mode"))
    {
        return 1;
    }
    const std::regex numa_regex("numa\\d+_cores");
    int count;
    for (const auto& file :
         std::filesystem::directory_iterator(fmt::format("/sys/class/ve/ve{}", device_id)))
    {
        if (std::regex_match(file.path().string(), numa_regex))
        {
            count++;
        }
    }
    return count;
}

std::vector<Device> get_nec_devices()
{
    std::regex nec_device_regex("/sys/class/ve/ve(\\d+)");
    std::smatch nec_device_match;

    std::vector<Device> devices;
    for (const auto& file : std::filesystem::directory_iterator("/sys/class/ve/"))
    {
        std::string file_path = file.path().string();
        if (std::regex_match(file_path, nec_device_match, nec_device_regex))
        {
            devices.emplace_back(Device(std::stoi(nec_device_match[1].str())));
        }
    }

    return devices;
}
} // namespace nec
} // namespace lo2s
