#include <lo2s/config.hpp>
#include <lo2s/perf/bio/event_cacher.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <fmt/core.h>

#include <string>

extern "C"
{
#include <sys/stat.h>
#include <sys/sysmacros.h>
}

namespace lo2s
{
namespace perf
{
namespace bio
{

EventCacher::EventCacher(Cpu cpu, std::map<dev_t, std::unique_ptr<Writer>>& writers,
                         BioEventType type)
: Reader(cpu, type), writers_(writers)
{
    for (auto& entry : get_block_devices())
    {
        events_.emplace(std::piecewise_construct, std::forward_as_tuple(entry.id), std::tuple());

        events_[entry.id].reserve(config().block_io_cache_size);
    }
}

bool EventCacher::handle(const Reader::RecordSampleType* sample)
{
    struct RecordBlock* record = (struct RecordBlock*)&sample->tp_data;
    otf2::common::io_operation_mode_type mode = otf2::common::io_operation_mode_type::flush;
    // TODO: Handle the few io operations that arent either reads or write
    if (record->rwbs[0] == 'R')
    {
        mode = otf2::common::io_operation_mode_type::read;
    }
    else if (record->rwbs[0] == 'W')
    {
        mode = otf2::common::io_operation_mode_type::write;
    }
    else
    {
        return false;
    }

    dev_t dev = makedev(record->dev >> 20, record->dev & ((1U << 20) - 1));
    // Linux reports the size of  I/O operations in number of sectors, which are always 512 byte
    // lare, regardless of what the real sector size of the block device is
    events_[dev].push_back(
        BioEvent(dev, record->sector, sample->time, type_, mode, record->nr_sector * 512));

    if (events_[dev].size() == config().block_io_cache_size)
    {
        writers_[dev]->submit_events(events_[dev]);
        events_[dev].clear();
    }
    return false;
}

} // namespace bio
} // namespace perf
} // namespace lo2s
