#ifndef LO2S_PERF_SAMPLE_RAW_HPP
#define LO2S_PERF_SAMPLE_RAW_HPP

#include "event_format.hpp"

#include "../perf_event_reader.hpp"
#include "../util.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <ios>

extern "C" {
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <syscall.h>
}

namespace fs = boost::filesystem;

namespace lo2s
{
template <class T>
class perf_tracepoint_reader : public perf_event_reader<T>
{
public:
    struct record_dynamic_format
    {
        uint64_t get(const event_field& field) const
        {
            switch (field.size())
            {
            case 1:
                return _get<int8_t>(field.offset());
            case 2:
                return _get<int16_t>(field.offset());
            case 4:
                return _get<int32_t>(field.offset());
            case 8:
                return _get<int64_t>(field.offset());
            default:
                // We do check this before setting up the event
                assert(!"Trying to get field of invalid size.");
                return 0;
            }
        }

        template <typename TT>
        const TT _get(ptrdiff_t offset) const
        {
            assert(offset >= 0);
            assert(offset + sizeof(TT) <= size_);
            return *(reinterpret_cast<const TT*>(raw_data_ + offset));
        }

        // DO NOT TOUCH, MUST NOT BE size_t!!!!
        uint32_t size_;
        char raw_data_[1]; // Can I still not [0] with ISO-C++ :-(
    };

    struct record_sample_type
    {
        struct perf_event_header header;
        uint64_t time;
        // uint32_t size;
        // char data[size];
        record_dynamic_format raw_data;
    };

    using perf_event_reader<T>::init_mmap;

    perf_tracepoint_reader(int cpu, int event_id, size_t mmap_pages) : cpu_(cpu)
    {
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.size = sizeof(attr);
        attr.config = event_id;
        attr.disabled = 1;
        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME;

        attr.watermark = 1;
        attr.wakeup_watermark = static_cast<uint32_t>(0.8 * mmap_pages * get_page_size());

        fd_ = syscall(__NR_perf_event_open, &attr, -1, cpu_, -1, 0);
        if (fd_ < 0)
        {
            log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }
        log::debug() << "Opened perf_sample_tracepoint_reader for cpu " << cpu_ << " with id "
                     << event_id;

        try
        {
            // asynchronous delivery
            // if (fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK))
            if (fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_, mmap_pages);
            log::debug() << "perf_tracepoint_reader mmap initialized";

            auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
            log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
            if (ret == -1)
            {
                throw_errno();
            }
        }
        catch (...)
        {
            close(fd_);
            throw;
        }
    }

    perf_tracepoint_reader(perf_tracepoint_reader&& other)
    : perf_event_reader<T>(std::forward<perf_event_reader<T>>(other)), cpu_(other.cpu_)
    {
        std::swap(fd_, other.fd_);
    }

    ~perf_tracepoint_reader()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    int fd() const
    {
        return fd_;
    }

    void stop()
    {
        auto ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE);
        log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_DISABLE) = " << ret;
        if (ret == -1)
        {
            throw_errno();
        }
        this->read();
    }

private:
    int cpu_;
    int fd_ = -1;
    const static fs::path base_path;
};

template <typename T>
const fs::path perf_tracepoint_reader<T>::base_path = fs::path("/sys/kernel/debug/tracing/events");
}
#endif // LO2S_PERF_SAMPLE_RAW_HPP
