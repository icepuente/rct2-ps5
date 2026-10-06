/*
 * Report how much direct memory a payload can allocate.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

size_t sceKernelGetDirectMemorySize(void);
int sceKernelAvailableDirectMemorySize(off_t, off_t, size_t, off_t *, size_t *);
int sceKernelAllocateMainDirectMemory(size_t, size_t, int, intptr_t *);
int sceKernelAllocateDirectMemory(off_t, off_t, size_t, size_t, int, intptr_t *);
int sceKernelReleaseDirectMemory(intptr_t, size_t);

int main(void)
{
    size_t total = sceKernelGetDirectMemorySize();
    off_t start = 0;
    size_t avail = 0;
    int rc = sceKernelAvailableDirectMemorySize(0, total, 0x10000, &start, &avail);
    printf("direct memory: total %zu MiB, largest free block %zu MiB (rc=%x)\n",
           total >> 20, avail >> 20, rc);

    static const size_t sizes_mib[] = { 64, 32, 24, 16, 8, 4, 2 };
    for (size_t i = 0; i < sizeof(sizes_mib) / sizeof(sizes_mib[0]); i++) {
        size_t len = sizes_mib[i] << 20;
        intptr_t paddr = 0;

        rc = sceKernelAllocateMainDirectMemory(len, 0x20000, 3, &paddr);
        printf("AllocateMainDirectMemory(%3zu MiB): %s\n", sizes_mib[i],
               rc ? strerror(errno) : "ok");
        if (rc == 0) {
            sceKernelReleaseDirectMemory(paddr, len);
        }

        rc = sceKernelAllocateDirectMemory(0, total, len, 0x20000, 3, &paddr);
        printf("AllocateDirectMemory    (%3zu MiB): %s\n", sizes_mib[i],
               rc ? strerror(errno) : "ok");
        if (rc == 0) {
            sceKernelReleaseDirectMemory(paddr, len);
        }
    }
    return 0;
}
