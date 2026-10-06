/*
 * Emulated thread-local storage (__emutls_get_address).
 *
 * The payload SDK's libc++ and libunwind are built with -femulated-tls, and a
 * native title has no runtime that provides it. Same layout and behaviour as
 * LLVM compiler-rt's emutls: each thread keeps an array of per-variable blocks
 * under one pthread key, indexed by an id assigned on first use.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct __emutls_control {
    size_t size;
    size_t align;
    union {
        uintptr_t index;
        void *address;
    } object;
    void *value;
} __emutls_control;

typedef struct {
    size_t count;
    void *data[];
} emutls_array;

static pthread_key_t emutls_key;
static pthread_once_t emutls_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t emutls_lock = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t emutls_next_index;

static void emutls_destroy(void *ptr)
{
    emutls_array *array = ptr;
    for (size_t i = 0; i < array->count; i++) {
        if (array->data[i]) {
            free(((void **)array->data[i])[-1]);
        }
    }
    free(array);
}

static void emutls_init(void)
{
    pthread_key_create(&emutls_key, emutls_destroy);
}

static uintptr_t emutls_index(__emutls_control *control)
{
    uintptr_t index = __atomic_load_n(&control->object.index, __ATOMIC_ACQUIRE);
    if (index == 0) {
        pthread_mutex_lock(&emutls_lock);
        index = control->object.index;
        if (index == 0) {
            index = ++emutls_next_index;
            __atomic_store_n(&control->object.index, index, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&emutls_lock);
    }
    return index;
}

/* Allocate an aligned block, keeping the raw pointer just before it. */
static void *emutls_allocate(__emutls_control *control)
{
    size_t align = control->align < sizeof(void *) ? sizeof(void *) : control->align;
    char *raw = malloc(control->size + align + sizeof(void *));
    if (!raw) {
        abort();
    }
    uintptr_t base = (uintptr_t)(raw + sizeof(void *));
    void *object = (void *)((base + align - 1) & ~(uintptr_t)(align - 1));
    ((void **)object)[-1] = raw;
    if (control->value) {
        memcpy(object, control->value, control->size);
    } else {
        memset(object, 0, control->size);
    }
    return object;
}

void *__emutls_get_address(__emutls_control *control)
{
    pthread_once(&emutls_once, emutls_init);
    uintptr_t index = emutls_index(control);

    emutls_array *array = pthread_getspecific(emutls_key);
    if (!array || array->count < index) {
        size_t count = index + 16;
        size_t old = array ? array->count : 0;
        emutls_array *grown = realloc(array, sizeof(*array) + count * sizeof(void *));
        if (!grown) {
            abort();
        }
        memset(grown->data + old, 0, (count - old) * sizeof(void *));
        grown->count = count;
        array = grown;
        pthread_setspecific(emutls_key, array);
    }

    void **slot = &array->data[index - 1];
    if (!*slot) {
        *slot = emutls_allocate(control);
    }
    return *slot;
}
