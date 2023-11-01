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

#include <lo2s/log.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/time/converter.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <functional>
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
    MultiReader(trace::Trace& trace) : writer_(trace)
    {
        for (const auto& cpu : Topology::instance().cpus())
        {
            for (auto tp : writer_.get_tracepoints())
            {
                IoReaderIdentity id(tp, cpu);
                auto reader = readers_.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                                               std::forward_as_tuple(id));
                fds_.emplace_back(std::cref(reader.first->second.fd()));
            }
        }
    }

    MultiReader(const MultiReader& other) = delete;
    MultiReader& operator=(const MultiReader&) = delete;

    MultiReader(MultiReader&& other) = default;
    MultiReader& operator=(MultiReader&&) = default;
    ~MultiReader() = default;

public:
    void read()
    {
        std::priority_queue<ReaderState, std::vector<ReaderState>, std::greater<ReaderState>>
            earliest_available;

        for (auto& reader : readers_)
        {
            if (!reader.second.empty())
            {
                earliest_available.push(ReaderState(reader.second.top()->time, reader.first));
            }
        }

        while (!earliest_available.empty())
        {
            auto state = earliest_available.top();
            earliest_available.pop();

            while (!readers_.at(state.identity).empty())
            {
                auto event = readers_.at(state.identity).top();
                if (event->time > earliest_available.top().time)
                {
                    state.time = event->time;
                    earliest_available.push(state);
                    readers_.at(state.identity).pop();
                    break;
                }

                if (event->time < highest_written_)
                {
                    // OTF2 requires strict temporal event ordering. Because we combine multiple
                    // indepedent perf buffers, we can not guarantee that we will see the events for
                    // a block device in strict temporal order. In the rare case that we get an
                    // event with a smaller timestamp than an already written event, we have to just
                    // drop it.
                    readers_.at(state.identity).pop();
                    Log::warn() << "Event loss due to event arriving late!";
                    continue;
                }

                writer_.write(state.identity, event);
                highest_written_ = event->time;
                readers_.at(state.identity).pop();
            }
        }
    }

    void finalize()
    {
        for (auto& reader : readers_)
        {
            reader.second.stop();
        }
        // Flush the event buffer one last time
        read();
    }

    const std::vector<std::reference_wrapper<const Fd>> get_fds() const
    {
        return fds_;
    }

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

    std::vector<std::reference_wrapper<const Fd>> fds_;
};

} // namespace perf
} // namespace lo2s
