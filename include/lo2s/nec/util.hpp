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

uint64_t readSensor(int device_id, std::string sensor_name, bool is_hex = false)
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
        if (std::regex_match(file.path().numa_regex))
        {
            count++;
        }
    }
    return count;
}
class Sensor
{
public:
    Sensor(int device_id, std::string name) : device_id_(device_id), name_(name)
    {
    }
    virtual double read() = 0;

    const std::string name()
    {
        return fmt::format("{} ({})", name_, device_id_);
    }

protected:
    int device_id_;
    std::string name_;
};
class SimpleSensor : public Sensor
{
public:
    SimpleSensor(int device_id, std::string name, std::string file, double scaling)
    : Sensor(device_id, name), file_(file), scaling_(scaling)
    {
    }

    double read() override
    {
        return readSensor(device_id_, file_) / scaling_;
    }

private:
    std::string file_;
    double scaling_;
};

class TempSensor : public Sensor
{
public:
    TempSensor(int device_id, int core_id)
    : Sensor(device_id, fmt::format("core {} temperature", core_id)), core_id_(core_id)
    {
    }
    double read() override
    {
        return readSensor(device_id_, fmt::format("sensor_{}", core_id_ + 14)) / 1000000.0f;
    }

private:
    int core_id_;
};
class Device
{
public:
    Device(int device_id)
    {
        // TODO: You can read numaX_cores to get which NUMA area a core belongs to, is that useful
        // here?
        int enabled_cores = readSensor(device_id, "cores_enable", true);
        int bit = 1;

        for (int core_id = 0; core_id < sizeof(int) * 8; core_id++, bit <<= 1)
        {
            if (bit & enabled_cores)
            {
                sensors_.emplace_back(TempSensor(device_id, core_id));
            }
        }
        sensors_.emplace_back(SimpleSensor(device_id, "voltage", "sensor_8", 1000000.0f));
    }

    const std::vector<Sensor> sensors()
    {
        return sensors_;
    }

private:
    std::vector<Sensor> sensors_;
};
} // namespace nec
} // namespace lo2s
