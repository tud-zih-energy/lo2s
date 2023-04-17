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

uint64_t readSensor(int device_id, std::string sensor_name, bool is_hex = false);

int numaCount(int device_id);

class Sensor
{
public:
    Sensor(int device_id, std::string name) : device_id_(device_id), name_(name)
    {
    }

    virtual double read() const
    {
        return 0;
    }

    const std::string name() const
    {
        return fmt::format("{} ({})", name_, device_id_);
    }

    const std::string unit() const
    {
        return "#";
    }

    friend bool operator<(const Sensor& lhs, const Sensor& rhs)
    {
        return lhs.name_ < rhs.name_;
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

    double read() const override
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
    double read() const override
    {
        return readSensor(device_id_, fmt::format("sensor_{}", core_id_ + 14)) / 1000000.0f;
    }

private:
    int core_id_;
};
class Device
{
public:
    Device(int device_id) : device_id_(device_id)
    {
        // TODO: You can read numaX_cores to get which NUMA area a core belongs to, is that useful
        // here?
        int enabled_cores = readSensor(device_id, "cores_enable", true);
        int bit = 1;

        for (unsigned int core_id = 0; core_id < sizeof(int) * 8; core_id++, bit <<= 1)
        {
            if (bit & enabled_cores)
            {
                sensors_.emplace_back(TempSensor(device_id, core_id));
            }
        }
        sensors_.emplace_back(SimpleSensor(device_id, "voltage", "sensor_8", 1000000.0f));
    }

    const std::vector<Sensor> sensors() const
    {
        return sensors_;
    }

    const std::string name() const
    {
        return fmt::format("NEC Accelerator {}", device_id_);
    }

    int id() const
    {
        return device_id_;
    }

    friend bool operator<(const Device& lhs, const Device& rhs)
    {
        return lhs.device_id_ < rhs.device_id_;
    }

private:
    int device_id_;
    std::vector<Sensor> sensors_;
};

std::vector<Device> get_nec_devices();
} // namespace nec
} // namespace lo2s
