#include "x86_64_get_fs_base_helper.h"

#include <unistd.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#include <cstdio>
#include <cstdlib>

namespace PochiVM
{

uint64_t x86_64_get_fs_segmentation_register_base()
{
    unsigned long out = 0;
    long ret = syscall(__NR_arch_prctl, ARCH_GET_FS, &out);
    if (ret != 0)
    {
        fprintf(stderr, "[FATAL ERROR] Failed to retrieve FS segmentation register: "
                        "system call 'syscall(__NR_arch_prctl, ARCH_GET_FS, out)' returned error %ld\n", ret);
        abort();
    }
    return static_cast<uint64_t>(out);
}

}   // namespace PochiVM
