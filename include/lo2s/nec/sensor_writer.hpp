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

#pragma once
#include <lo2s/nec/util.hpp>

#include <otf2xx/otf2.hpp>

namespace lo2s
{
namespace nec
{

class SensorWriter
{
public:
    SensorWriter(trace::Trace& trace);

    void write();

private:
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
    std::vector<Device> devices_;
};
} // namespace nec
} // namespace lo2s
