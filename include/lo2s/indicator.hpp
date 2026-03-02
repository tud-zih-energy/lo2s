// SPDX-FileCopyrightText: 2025 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
