// SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
