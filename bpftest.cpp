/*
 * USDT python minimal working example
 * Python needs to be compiled with dtrace support for this. This requires SystemTap on Linux (which
 * most distros dont do)
 * ./configure --with-dtrace
 * Then compile this little program as: g++ bpftest.cpp -o bpftest -lbcc
 */

#define PY_BIN "/home/cvonelm/projects/cpython/python"
#include <functional>
#include <iostream>
#include <signal.h>
#include <vector>

#include <bcc/BPF.h>
#include <bpf/libbpf.h>

const std::string BPF_PROGRAM = R"(
#include <linux/sched.h>

#include <uapi/linux/ptrace.h>
struct event_t {
    bool type;
    int cpu;
    u64 time;
	char filename[32];
    char funcname[32];
};
BPF_RINGBUF_OUTPUT(events, 8);
int on_function__entry(struct pt_regs *ctx) {
  struct event_t event = {};
  char *filename_ptr;
  char *funcname_ptr;
  event.type = false;
  bpf_usdt_readarg(1, ctx, &filename_ptr);
  bpf_usdt_readarg(2, ctx, &funcname_ptr);
  bpf_probe_read_user_str(event.filename, 32, filename_ptr);
  bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
  event.time = bpf_ktime_get_ns();
  event.cpu = ((struct task_struct *)bpf_get_current_task())->cpu;
  events.ringbuf_output(&event, sizeof(event), 0);
  return 0;
}

int on_function__return(struct pt_regs *ctx) {
  struct event_t event = {};
  char *filename_ptr;
  char *funcname_ptr;
  event.type = true;
  bpf_usdt_readarg(1, ctx, &filename_ptr);
  bpf_usdt_readarg(2, ctx, &funcname_ptr);
  bpf_probe_read_user_str(event.filename, 32, filename_ptr);
  bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
  event.time = bpf_ktime_get_ns();
  event.cpu = ((struct task_struct *)bpf_get_current_task())->cpu;
  events.ringbuf_output(&event, sizeof(event), 0);
  return 0;
}
)";

struct event_t
{
    bool type;
    int cpu;
    uint64_t time;
    char filename[32];
    char funcname[16];
};

int ident = 0;
int handle_output(void* ctx, void* data, size_t data_size)
{

    auto event = static_cast<event_t*>(data);
    if (event->type == false) // ENTER
    {
        for (int i = 0; i < ident; i++)
        {
            std::cout << " ";
        }
        std::cout << "ENTER: (" << event->time << "):" << event->filename << "." << event->funcname
                  << "(ON CPU:" << event->cpu << ")" << std::endl;
        ident++;
    }
    else
    {
        ident--;
        if (ident < 0)
        {
            ident = 0;
        }

        for (int i = 0; i < ident; i++)
        {
            std::cout << " ";
        }
        std::cout << "LEAVE: (" << event->time << "):" << event->filename << "." << event->funcname
                  << std::endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    ebpf::USDT u1(PY_BIN, "python", "function__entry", "on_function__entry");
    ebpf::USDT u2(PY_BIN, "python", "function__return", "on_function__return");
    ebpf::BPF* bpf = new ebpf::BPF();

    auto init_res = bpf->init(BPF_PROGRAM, {}, { u1, u2 });
    if (init_res.code() != 0)
    {
        std::cerr << init_res.msg() << std::endl;
        return 1;
    }

    auto attach_res = bpf->attach_usdt_all();
    if (attach_res.code() != 0)
    {
        std::cerr << attach_res.msg() << std::endl;
        return 1;
    }

    int err = 0;
    struct ring_buffer* rb = NULL;

    rb = ring_buffer__new(bpf->get_table("events").get_fd(), handle_output, NULL, NULL);
    if (!rb)
    {
        err = -1;
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    while (true)
    {
        ring_buffer__poll(rb, 100 /* timeout, ms */);
    }
cleanup:
    ring_buffer__free(rb);

    return err < 0 ? -err : 0;
}
