#ifndef LO2S_PERF_SAMPLE_RAW_HPP
#define LO2S_PERF_SAMPLE_RAW_HPP

#include "../perf_event_reader.hpp"
#include "../util.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <ios>

extern "C" {
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <syscall.h>
}

namespace fs = boost::filesystem;

namespace lo2s
{
template <class T>
class perf_sample_raw_reader : public perf_event_reader<T>
{
public:
    struct record_power_cpu_idle
    {
        unsigned short common_type;
        unsigned char common_flags;
        unsigned char common_preempt_count;
        int common_pid;
        int32_t state;
        uint32_t cpu_id;
    };
    struct record_sample_type
    {
        struct perf_event_header header;
        uint64_t time;
        uint32_t size;
        // char data[size];
        record_power_cpu_idle data;
    };

    using perf_event_reader<T>::init_mmap;

    perf_sample_raw_reader(int cpu, size_t mmap_pages) : cpu_(cpu)
    {
        fs::path path_event = base_path / "power" / "cpu_idle";
        fs::ifstream ifs_id;
        ifs_id.exceptions(std::ios::failbit | std::ios::badbit);

        try
        {
            ifs_id.open(path_event / "id");
            ifs_id >> id_;
        }
        catch (...)
        {
            log::error() << "Couldn't read ID from " << (path_event / "id");
            throw;
        }

        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.size = sizeof(attr);
        attr.config = id_;
        attr.disabled = 1;
        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME;

        attr.watermark = 1;
        attr.wakeup_watermark = static_cast<uint32_t>(0.8 * mmap_pages * get_page_size());

        fd_ = syscall(__NR_perf_event_open, &attr, -1, cpu_, -1, 0);
        if (fd_ < 0)
        {
            log::error() << "perf_event_open for raw perf_sample_raw failed.";
            throw_errno();
        }
        log::debug() << "Opened perf_sample_raw_reader for cpu " << cpu_ << " with id " << id_;

        try
        {
            // asynchronous delivery
            // if (fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK))
            if (fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_, mmap_pages);
            log::debug() << "perf_sample_raw_reader mmap initialized";

            auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
            log::debug() << "perf_sample_raw_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
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

    perf_sample_raw_reader(perf_sample_raw_reader&& other)
    : perf_event_reader<T>(std::forward<perf_event_reader<T>>(other)), cpu_(other.cpu_),
      id_(other.id_)
    {
        std::swap(fd_, other.fd_);
    }

    ~perf_sample_raw_reader()
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
        log::debug() << "perf_sample_raw_reader ioctl(fd, PERF_EVENT_IOC_DISABLE) = " << ret;
        if (ret == -1)
        {
            throw_errno();
        }
        this->read();
    }

private:
    int cpu_;
    int id_;
    int fd_ = -1;
    const static fs::path base_path;
};

template <typename T>
const fs::path perf_sample_raw_reader<T>::base_path = fs::path("/sys/kernel/debug/tracing/events");
}
#endif // LO2S_PERF_SAMPLE_RAW_HPP
