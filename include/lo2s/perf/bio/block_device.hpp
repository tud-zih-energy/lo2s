/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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

#include <map>
#include <string>
#include <utility>

#include <fmt/core.h>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{

enum class BlockDeviceType
{
    PARTITION,
    DISK
};

struct BlockDevice
{
    BlockDevice() : id(0), type(BlockDeviceType::PARTITION), parent(0)
    {
    }

    BlockDevice(dev_t id, std::string name, BlockDeviceType type, dev_t parent)
    : id(id), name(std::move(name)), type(type), parent(parent)
    {
    }

    static BlockDevice partition(dev_t id, const std::string& name, dev_t parent)
    {
        return { id, name, BlockDeviceType::PARTITION, parent };
    }

    static BlockDevice disk(dev_t id, const std::string& name)
    {
        return { id, name, BlockDeviceType::DISK, 0 };
    }

    static BlockDevice block_device_for(dev_t device);

    friend bool operator<(const BlockDevice& lhs, const BlockDevice& rhs)
    {
        return lhs.id < rhs.id;
    }

    dev_t id;
    std::string name;
    BlockDeviceType type;
    dev_t parent;
};

std::map<dev_t, BlockDevice> get_block_devices();
} // namespace lo2s
