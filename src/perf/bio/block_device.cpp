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

#include <lo2s/perf/bio/block_device.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <string>

#include <cstdint>

#include <fmt/format.h>
#include <sys/types.h>

extern "C"
{
#include <sys/sysmacros.h>
}

namespace lo2s
{

std::map<dev_t, BlockDevice> get_block_devices()
{
    std::map<dev_t, BlockDevice> result;
    std::filesystem::path const sys_dev_path("/sys/dev/block");

    const std::regex devname_regex("DEVNAME=(\\S+)");
    const std::regex devtype_regex("DEVTYPE=(\\S+)");
    const std::regex major_regex("MAJOR=(\\S+)");
    const std::regex minor_regex("MINOR=(\\S+)");

    std::smatch devname_match;
    std::smatch devtype_match;
    std::smatch major_match;
    std::smatch minor_match;

    for (const std::filesystem::directory_entry& dir_entry :
         std::filesystem::directory_iterator(sys_dev_path))
    {
        std::string devname = "unknown device";
        uint32_t major = 0;
        uint32_t minor = 0;
        std::string devtype = "partition";

        std::filesystem::path const uevent_path = dir_entry.path() / "uevent";
        std::ifstream uevent_file(uevent_path);

        while (uevent_file.good())
        {
            std::string line;
            uevent_file >> line;
            if (std::regex_match(line, devname_match, devname_regex))
            {
                devname = fmt::format("/dev/{}", devname_match[1].str());
            }
            else if (std::regex_match(line, devtype_match, devtype_regex))
            {
                devtype = devtype_match[1].str();
            }
            else if (std::regex_match(line, major_match, major_regex))
            {
                major = std::stoi(major_match[1].str());
            }
            else if (std::regex_match(line, minor_match, minor_regex))
            {
                minor = std::stoi(minor_match[1].str());
            }
        }

        uint32_t parent_major = 0;
        uint32_t parent_minor = 0;
        if (devtype == "partition")
        {
            std::filesystem::path parent_dev("/sys");

            // Because someone at Linux has a serious glue-sniffing problem these symlinks are
            // relative paths and not absolute. Solution: delete the relative part "../../" from the
            // beginning and make it absolute
            parent_dev =
                parent_dev /
                (std::filesystem::read_symlink(dir_entry.path()).parent_path().string().substr(6));

            std::ifstream parent_uevent_file(parent_dev / "uevent");
            while (parent_uevent_file.good())
            {
                std::string line;
                parent_uevent_file >> line;

                if (std::regex_match(line, major_match, major_regex))
                {
                    parent_major = std::stoi(major_match[1].str());
                }
                else if (std::regex_match(line, minor_match, minor_regex))
                {
                    parent_minor = std::stoi(minor_match[1].str());
                }
            }
        }

        dev_t dev_id = makedev(major, minor);
        if (devtype == "partition")
        {
            result.emplace(dev_id, BlockDevice::partition(dev_id, devname,
                                                          makedev(parent_major, parent_minor)));
        }
        else
        {
            result.emplace(dev_id, BlockDevice::disk(dev_id, devname));
        }
    }
    return result;
}

BlockDevice BlockDevice::block_device_for(dev_t device)
{
    static std::map<dev_t, BlockDevice> devices{ get_block_devices() };
    if (devices.count(device))
    {
        return devices[device];
    }

    return BlockDevice::disk(device, fmt::format("{}:{}", major(device), minor(device)));
}
} // namespace lo2s
