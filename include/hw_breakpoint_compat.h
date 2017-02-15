#ifndef LO2S_HW_BREAKPOINT_COMPAT_H
#define LO2S_HW_BREAKPOINT_COMPAT_H

extern "C"
{

#ifndef HW_BREAKPOINT_COMPAT

#include <linux/hw_breakpoint.h>

#else

/* Evil workaround to enable HW breakpoint for old kernels with
backported support but missing headers. Damn you Red Hat */

enum {
    HW_BREAKPOINT_LEN_1 = 1,
    HW_BREAKPOINT_LEN_2 = 2,
    HW_BREAKPOINT_LEN_4 = 4,
    HW_BREAKPOINT_LEN_8 = 8,
};

enum {
    HW_BREAKPOINT_EMPTY     = 0,
    HW_BREAKPOINT_R         = 1,
    HW_BREAKPOINT_W         = 2,
    HW_BREAKPOINT_RW        = HW_BREAKPOINT_R | HW_BREAKPOINT_W,
    HW_BREAKPOINT_X         = 4,
    HW_BREAKPOINT_INVALID   = HW_BREAKPOINT_RW | HW_BREAKPOINT_X,
};

#endif
}
#endif /* LO2S_HW_BREAKPOINT_COMPAT_H */
