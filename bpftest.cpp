/*
 * USDT python minimal working example
 * Python needs to be compiled with dtrace support for this. This requires SystemTap on Linux (which
 * most distros dont do)
 * ./configure --with-dtrace
 * Then compile this little program as: g++ bpftest.cpp -o bpftest -lbcc
 */

#define PY_BIN "path/to/your/dtrace/python/binary/here"
#include <functional>
#include <iostream>
#include <signal.h>
#include <vector>

#include <bcc/BPF.h>

const std::string BPF_PROGRAM = R"(
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>
struct event_t {
	char name[16];
};
BPF_PERF_OUTPUT(events);
int on_function__entry(struct pt_regs *ctx) {
  struct event_t event = {};
  char *usr_ptr;
  bpf_usdt_readarg(2, ctx, &usr_ptr);
  bpf_probe_read_user_str(event.name, 16, usr_ptr);
  
  events.perf_submit(ctx, &event, sizeof(event));
  return 0;
}
)";

struct event_t
{
    char name[16];
};

void handle_output(void* cb_cookie, void* data, int data_size)
{
    auto event = static_cast<event_t*>(data);
    std::cout << "func name: " << event->name << std::endl;
}

std::function<void(int)> shutdown_handler;

void signal_handler(int s)
{
    shutdown_handler(s);
}

int main(int argc, char** argv)
{
    ebpf::USDT u(PY_BIN, "python", "function__entry", "on_function__entry");

    ebpf::BPF* bpf = new ebpf::BPF();

    auto init_res = bpf->init(BPF_PROGRAM, {}, { u });
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
    else
    {
        std::cout << "Attached to USDT " << u;
    }

    auto open_res = bpf->open_perf_buffer("events", &handle_output);
    if (open_res.code() != 0)
    {
        std::cerr << open_res.msg() << std::endl;
        return 1;
    }

    shutdown_handler = [&](int s) {
        std::cerr << "Terminating..." << std::endl;
        bpf->detach_usdt_all();
        delete bpf;
        exit(0);
    };
    signal(SIGINT, signal_handler);

    std::cout << "Started tracing, hit Ctrl-C to terminate." << std::endl;
    auto perf_buffer = bpf->get_perf_buffer("events");
    if (perf_buffer)
        while (true)
            // 100ms timeout
            perf_buffer->poll(100);

    return 0;
}
