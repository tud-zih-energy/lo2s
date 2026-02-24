/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2025,
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

#include <algorithm>
#include <iostream>

#include <cmath>
#include <cstdint>

extern "C"
{

#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <unistd.h>
}

namespace lo2s
{
/*
 * Prints a Progress bar of the style: [###   ]
 */
class Indicator
{
public:
    Indicator()
    {
        draw();
    }

    void submit_status(uint64_t now, uint64_t total)
    {
        if (total == 0)
        {
            return;
        }
        progress_ = static_cast<double>(now) / total;
        draw();
    }

private:
    void draw()
    {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        if (w.ws_col < 10)
        {
            // Don't draw if screen is smaller than 10 chars
            return;
        }

        if (already_drawn_)
        {
            std::cout << '\r';
        }
        else
        {
            std::cout << '\n';
        }

        std::cout << "[";

        const uint64_t max_bar_length = w.ws_col - 2;

        uint64_t filled_bar_length = 0;
        if (progress_ > 0.0)
        {
            filled_bar_length =
                static_cast<uint64_t>(floor(static_cast<double>(max_bar_length) * progress_));
            filled_bar_length = std::min(max_bar_length, filled_bar_length);
        }
        uint64_t const free_bar_length =
            std::max(max_bar_length - filled_bar_length, static_cast<uint64_t>(0));

        for (uint64_t i = 0; i < filled_bar_length; i++)
        {
            std::cout << '#';
        }

        for (uint64_t i = 0; i < free_bar_length; i++)
        {
            std::cout << ' ';
        }

        std::cout << ']' << std::flush;

        already_drawn_ = true;
    }

    double progress_ = 0.0;
    bool already_drawn_ = false;
};
} // namespace lo2s
