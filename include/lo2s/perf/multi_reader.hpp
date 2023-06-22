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

#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/time/converter.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <queue>
#include <unordered_map>
#include <vector>

namespace lo2s
{
namespace perf
{

template <class Writer>
class MultiReader
{
public:
    MultiReader(trace::Trace& trace);

    MultiReader(const MultiReader& other) = delete;
    MultiReader& operator=(const MultiReader&) = delete;

    MultiReader(MultiReader&& other) = default;
    MultiReader& operator=(MultiReader&&) = default;
    ~MultiReader() = default;

public:
    void read();

    void finalize()
    {
        for (auto& reader : readers_)
        {
            reader.second.stop();
        }
        // Flush the event buffer one last time
        read();
    }

    std::vector<int> get_fds();

private:
    struct ReaderState
    {
        ReaderState(uint64_t time, IoReaderIdentity identity) : time(time), identity(identity)
        {
        }

        uint64_t time;
        IoReaderIdentity identity;

        friend bool operator>(const ReaderState& lhs, const ReaderState& rhs)
        {
            if (lhs.time == rhs.time)
            {
                return lhs.identity > rhs.identity;
            }
            return lhs.time > rhs.time;
        }
    };

    Writer writer_;
    std::map<IoReaderIdentity, IoReader> readers_;
    uint64_t highest_written_ = 0;
    std::priority_queue<ReaderState, std::vector<ReaderState>, std::greater<ReaderState>>
        earliest_available_;

    std::vector<int> fds_;
};

} // namespace perf
} // namespace lo2s
