/*
 * Application heap for native PS5 titles.
 *
 * The libc heap a native title gets is far smaller than its flexible memory
 * budget (an 8 MB SDL surface already fails), so the allocation functions are
 * redirected (lld --wrap, see scripts/package-native.sh) to an mspace on a
 * large flexible memory mapping. Blocks that the console's libc allocated
 * itself (strdup, getline, ...) are recognised by address and handed back to
 * the original functions.
 */
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void *SceLibcMspace;

SceLibcMspace sceLibcMspaceCreate(const char *, void *, size_t, unsigned int);
void *sceLibcMspaceMalloc(SceLibcMspace, size_t);
void *sceLibcMspaceCalloc(SceLibcMspace, size_t, size_t);
void *sceLibcMspaceRealloc(SceLibcMspace, void *, size_t);
void *sceLibcMspaceMemalign(SceLibcMspace, size_t, size_t);
int sceLibcMspacePosixMemalign(SceLibcMspace, void **, size_t, size_t);
int sceLibcMspaceFree(SceLibcMspace, void *);
size_t sceLibcMspaceMallocUsableSize(void *);

int sceKernelAvailableFlexibleMemorySize(size_t *);
int sceKernelMapNamedFlexibleMemory(void **, size_t, int, int, const char *);
int sceKernelDebugOutText(int, const char *);

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void *__real_memalign(size_t, size_t);
int __real_posix_memalign(void **, size_t, size_t);
void *__real_aligned_alloc(size_t, size_t);
void __real_free(void *);
size_t __real_malloc_usable_size(void *);

/* Flexible memory left to the console's libc and system modules. */
#define SYSTEM_RESERVE ((size_t)64 << 20)
#define MAP_ALIGN ((size_t)2 << 20)
#define PROT_CPU_RW 0x3

static SceLibcMspace heap;
static uintptr_t heap_start, heap_end;
static atomic_int heap_state; /* 0 = not started, 1 = starting, 2 = ready, 3 = failed */

static void heap_log(const char *msg)
{
    sceKernelDebugOutText(0, msg);
}

static int heap_ready(void)
{
    int state = atomic_load(&heap_state);

    if (state >= 2) {
        return state == 2;
    }
    int expected = 0;
    if (!atomic_compare_exchange_strong(&heap_state, &expected, 1)) {
        while ((state = atomic_load(&heap_state)) == 1) {
        }
        return state == 2;
    }

    char msg[128];
    size_t avail = 0;
    void *base = NULL;
    sceKernelAvailableFlexibleMemorySize(&avail);
    size_t size = avail > SYSTEM_RESERVE ? (avail - SYSTEM_RESERVE) & ~(MAP_ALIGN - 1) : 0;

    if (size == 0 ||
        sceKernelMapNamedFlexibleMemory(&base, size, PROT_CPU_RW, 0, "app heap") != 0 ||
        !(heap = sceLibcMspaceCreate("app heap", base, size, 0))) {
        snprintf(msg, sizeof(msg), "ps5_heap: failed (%zu MiB available), using libc heap\n",
                 avail >> 20);
        heap_log(msg);
        atomic_store(&heap_state, 3);
        return 0;
    }

    heap_start = (uintptr_t)base;
    heap_end = heap_start + size;
    snprintf(msg, sizeof(msg), "ps5_heap: %zu MiB at %p\n", size >> 20, base);
    heap_log(msg);
    atomic_store(&heap_state, 2);
    return 1;
}

static int in_heap(const void *p)
{
    return (uintptr_t)p >= heap_start && (uintptr_t)p < heap_end;
}

void *__wrap_malloc(size_t size)
{
    return heap_ready() ? sceLibcMspaceMalloc(heap, size) : __real_malloc(size);
}

void *__wrap_calloc(size_t n, size_t size)
{
    return heap_ready() ? sceLibcMspaceCalloc(heap, n, size) : __real_calloc(n, size);
}

void *__wrap_memalign(size_t align, size_t size)
{
    return heap_ready() ? sceLibcMspaceMemalign(heap, align, size)
                        : __real_memalign(align, size);
}

void *__wrap_aligned_alloc(size_t align, size_t size)
{
    return heap_ready() ? sceLibcMspaceMemalign(heap, align, size)
                        : __real_aligned_alloc(align, size);
}

int __wrap_posix_memalign(void **out, size_t align, size_t size)
{
    return heap_ready() ? sceLibcMspacePosixMemalign(heap, out, align, size)
                        : __real_posix_memalign(out, align, size);
}

void __wrap_free(void *p)
{
    if (!p) {
        return;
    }
    if (in_heap(p)) {
        sceLibcMspaceFree(heap, p);
    } else {
        __real_free(p);
    }
}

size_t __wrap_malloc_usable_size(void *p)
{
    if (!p) {
        return 0;
    }
    return in_heap(p) ? sceLibcMspaceMallocUsableSize(p) : __real_malloc_usable_size(p);
}

void *__wrap_realloc(void *p, size_t size)
{
    if (!p) {
        return __wrap_malloc(size);
    }
    if (in_heap(p)) {
        return sceLibcMspaceRealloc(heap, p, size);
    }
    if (!heap_ready()) {
        return __real_realloc(p, size);
    }

    /* Move a block the console's libc allocated into the app heap. */
    void *q = sceLibcMspaceMalloc(heap, size);
    if (q) {
        size_t old = __real_malloc_usable_size(p);
        memcpy(q, p, old < size ? old : size);
        __real_free(p);
    }
    return q;
}
