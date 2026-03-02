// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/trace/fwd.hpp>

#include <memory>
#include <vector>

namespace lo2s::metric::plugin
{

class Plugin;

class Metrics
{
public:
    Metrics(trace::Trace& trace);

    Metrics(Metrics&) = delete;
    Metrics(Metrics&&) = delete;

    Metrics& operator=(Metrics&) = delete;
    Metrics& operator=(Metrics&&) = delete;

    ~Metrics();

    void start();
    void stop();

private:
    trace::Trace& trace_;
    std::vector<std::unique_ptr<Plugin>> metric_plugins_;
    bool running_ = false;
};
} // namespace lo2s::metric::plugin
