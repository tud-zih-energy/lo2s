#include <lo2s/rb/writer.hpp>

#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>
#include <lo2s/types/process.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

extern "C"
{
#include <sys/socket.h>
#include <sys/un.h>
}

namespace lo2s
{
RingbufWriter::RingbufWriter(Process process, RingbufMeasurementType type)
{
    constexpr int DEFAULT_PAGE_NUM = 16;
    int const fd = memfd_create("lo2s", 0);

    if (fd == -1)
    {
        throw ::std::system_error(errno, std::system_category());
    }

    size_t const pagesize = sysconf(_SC_PAGESIZE);
    int size = 0;

    int pages = DEFAULT_PAGE_NUM;

    char const* lo2s_rb_size = getenv("LO2S_RB_SIZE");
    if (lo2s_rb_size != nullptr)
    {
        pages = std::stoi(lo2s_rb_size);
        if (pages < 1)
        {
            throw std::runtime_error("Invalid amount of ringbuffer pages: " +
                                     std::to_string(pages));
        }
    }

    size = static_cast<int>(pagesize) * pages;

    if (ftruncate(fd, size + sysconf(_SC_PAGESIZE)) == -1)
    {
        close(fd);
        throw std::system_error(errno, std::system_category());
    }

    auto header_map = SharedMemory(fd, sizeof(struct ringbuf_header), 0);

    auto* first_header = header_map.as<struct ringbuf_header>();

    memset((void*)first_header, 0, sizeof(struct ringbuf_header));

    first_header->version = RINGBUF_VERSION;
    first_header->size = size;
    first_header->tail.store(0);
    first_header->head.store(0);

    first_header->pid = process.as_int();

    rb_ = std::make_unique<ShmRingbuf>(fd);
    write_fd(type);

    // Wait for other side to open connection
    while (!rb_->header()->lo2s_ready.load())
    {
    };
}

void RingbufWriter::commit()
{
    assert(reserved_size_ != 0);
    rb_->advance_head(reserved_size_);
    reserved_size_ = 0;
}

uint64_t RingbufWriter::timestamp()
{
    constexpr uint64_t NSEC_IN_SEC = 1000000000;
    assert(rb_->header()->lo2s_ready.load());
    struct timespec ts; // NOLINT
    clock_gettime(rb_->header()->clockid, &ts);
    uint64_t const res = (ts.tv_sec * NSEC_IN_SEC) + ts.tv_nsec;
    return res;
}

// Writes the fd of the shared memory to the Unix Domain Socket
void RingbufWriter::write_fd(RingbufMeasurementType type)
{
    char const* socket_path = getenv("LO2S_SOCKET");
    if (socket_path == nullptr)
    {
        throw std::runtime_error("LO2S_SOCKET is not set!");
    }

    int const data_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (data_socket == -1)
    {
        throw std::system_error(errno, std::system_category());
    }

    struct sockaddr_un addr; // NOLINT
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    int const ret =
        connect(data_socket, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
    if (ret < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    union // NOLINT
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    struct msghdr msg; // NOLINT
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;

    *CMSG_DATA(cmptr) = rb_->fd();

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;

    // We need to send some data with the fd anyways, so use that to send
    // the type of the measurement that we are doing.
    struct iovec iov[1];
    iov[0].iov_base = &type;
    iov[0].iov_len = sizeof(uint64_t);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    sendmsg(data_socket, &msg, 0);

    close(data_socket);
}
} // namespace lo2s
