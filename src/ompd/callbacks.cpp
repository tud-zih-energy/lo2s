
#include <cstring>
#include <fmt/core.h>
#include <lo2s/bfd_resolve.hpp>
#include <map>
extern "C"
{
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <omp-tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

extern "C"
{
    struct _ompd_aspace_cont
    {
        int id;
    };

    struct _ompd_thread_cont
    {
        int id;
    };

    static pid_t ompd_pid;
    static std::map<pid_t, ompd_thread_context_t> ompd_threads;

    ompd_address_space_context_t acontext = { 42 };
    void* ompd_library = NULL;

    ompd_rc_t ompd_callback_alloc(ompd_size_t nbytes, void** ptr)
    {
        *ptr = malloc(nbytes);
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_free(void* ptr)
    {
        free(ptr);
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_get_thread_context(ompd_address_space_context_t* address_space_context,
                                               ompd_thread_id_t kind, ompd_size_t sizeof_thread_id,
                                               const void* thread_id,
                                               ompd_thread_context_t** thread_context)
    {
        if (kind != 0 && kind != 1)
        {
            return ompd_rc_unsupported;
        }
        long int tid;

        tid = *(const uint64_t*)thread_id;

        ompd_threads[tid] = { .id = (int)tid };

        *thread_context = &ompd_threads[tid];

        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_sizeof(ompd_address_space_context_t* address_space_context,
                                   ompd_device_type_sizes_t* sizes)
    {

        *sizes = { .sizeof_char = sizeof(char),
                   .sizeof_short = sizeof(short),
                   .sizeof_int = sizeof(int),
                   .sizeof_long = sizeof(long),
                   .sizeof_long_long = sizeof(long long),
                   .sizeof_pointer = sizeof(void*) };
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_symbol_addr(ompd_address_space_context_t* address_space_contxt,
                                        ompd_thread_context_t* thread_context,
                                        const char* symbol_name, ompd_address_t* symbol_addr,
                                        const char* file_name)
    {
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_print_string(const char* string, int category)
    {
        printf("%s", string);
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_sym_addr(ompd_address_space_context_t* context,
                                     ompd_thread_context_t* tcontext, const char* symbol_name,
                                     ompd_address_t* symbol_addr, const char* file_name)
    {
        symbol_addr->address = 0;
        printf("%s:%s\n", symbol_name, file_name);
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_read(ompd_address_space_context_t* context,
                                 ompd_thread_context_t* tcontext, const ompd_address_t* addr,
                                 ompd_size_t nbytes, void* buffer)
    {
        uint64_t readMem = (uint64_t)addr->address;
        int fd = open(fmt::format("/proc/{}/mem", ompd_pid).c_str(), 0, O_RDONLY);
        if (fd == -1)
        {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            return ompd_rc_error;
        }
        if (read(fd, buffer, nbytes) != nbytes)
        {
            fprintf(stderr, "Error: %s\n", strerror(errno));
        }
        close(fd);
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_read_string(ompd_address_space_context_t* context,
                                        ompd_thread_context_t* tcontext, const ompd_address_t* addr,
                                        ompd_size_t nbytes, void* buffer)
    {
        ompd_callback_read(context, tcontext, addr, nbytes, buffer);
        ((char*)buffer)[nbytes - 1] = '\0';
        return ompd_rc_ok;
    }

    ompd_rc_t ompd_callback_endianess(ompd_address_space_context_t* address_space_context,
                                      const void* input, ompd_size_t unit_size, ompd_size_t count,
                                      void* output)
    {
        memmove(output, input, count * unit_size);
        return ompd_rc_ok;
    }

    void call_ompd_initialize()
    {
        ompd_library = dlopen("/usr/lib/libompd.so", RTLD_LAZY);

        if (ompd_library == NULL)
        {
            fprintf(stderr, "Could not load libompd.so!");
        }

        static ompd_callbacks_t table = { ompd_callback_alloc,
                                          ompd_callback_free,
                                          ompd_callback_print_string,
                                          ompd_callback_sizeof,
                                          ompd_callback_sym_addr,
                                          ompd_callback_read,
                                          NULL,
                                          ompd_callback_read_string,
                                          ompd_callback_endianess,
                                          ompd_callback_endianess,
                                          ompd_callback_get_thread_context };

        ompd_rc_t (*my_ompd_init)(ompd_word_t version, ompd_callbacks_t*) =
            (ompd_rc_t(*)(ompd_word_t, ompd_callbacks_t*))dlsym(ompd_library, "ompd_initialize");
        ompd_rc_t returnInit = my_ompd_init(201811, &table);

        if (returnInit != ompd_rc_ok)
        {
            fprintf(stderr, "Error: %d\n", returnInit);
        }

        ompd_rc_t (*my_proc_init)(ompd_address_space_context_t*, ompd_address_space_handle_t**) =
            (ompd_rc_t(*)(ompd_address_space_context_t*, ompd_address_space_handle_t**))dlsym(
                ompd_library, "ompd_process_initialize");

        ompd_address_space_handle_t* addr_space = NULL;

        ompd_rc_t retProcInit = my_proc_init(&acontext, &addr_space);

        if (retProcInit != ompd_rc_ok)
        {
            fprintf(stderr, "Error (my_proc_init): %d\n", retProcInit);
        }
    }

    ompd_rc_t call_ompd_finalize()
    {
        static ompd_rc_t (*my_ompd_finalize)(void) = NULL;
        if (!my_ompd_finalize)
        {
            my_ompd_finalize = (ompd_rc_t(*)(void))dlsym(ompd_library, "ompd_finalize");
            if (dlerror())
            {
                return ompd_rc_error;
            }
        }
        return my_ompd_finalize();
    }
}
