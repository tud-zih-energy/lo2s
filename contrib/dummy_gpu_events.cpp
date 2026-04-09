// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/gpu/ringbuf.hpp>
#include <lo2s/types/process.hpp>

#include <memory>

extern "C"
{
#include <unistd.h>
}

int main()
{
    auto rb_writer = std::make_unique<lo2s::gpu::RingbufWriter>(lo2s::Process::me());
    uint64_t cctx = rb_writer->kernel_def("foobar");
    rb_writer->kernel(rb_writer->timestamp(), rb_writer->timestamp() + 100000000, cctx);
    return 0;
}
