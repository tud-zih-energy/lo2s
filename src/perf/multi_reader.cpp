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

#include <lo2s/perf/bio/reader.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/multi_reader.hpp>

namespace lo2s
{
namespace perf
{
template <class Reader, class Writer>
MultiReader<Reader, Writer>::MultiReader(trace::Trace& trace) : writer_(trace)
{
}

template <class Reader, class Writer>
int MultiReader<Reader, Writer>::addReader(ReaderIdentity identity)
{
    auto reader = readers_.emplace(std::piecewise_construct, std::forward_as_tuple(identity),
                                   std::forward_as_tuple(Reader(identity)));
    return reader.first->second.fd();
}

template <class Reader, class Writer>
void MultiReader<Reader, Writer>::read()
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
                // indepedent perf buffers, we can not guarantee that we will see the events for a
                // block device in strict temporal order. In the rare case that we get an event with
                // a smaller timestamp than an already written event, we have to just drop it.
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

template class MultiReader<bio::Reader, bio::Writer>;

} // namespace perf
} // namespace lo2s
