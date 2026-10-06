/*
 * __cxa_thread_atexit_impl: runs thread_local destructors when a thread exits.
 *
 * libc++abi references it weakly and normally gets it from the system libc;
 * the console's libc does not export it to a native title, and the native
 * converter rejects unresolved weak imports.
 */
#include <pthread.h>
#include <stdlib.h>

struct dtor_node {
    void (*dtor)(void *);
    void *obj;
    struct dtor_node *next;
};

static pthread_key_t dtor_key;
static pthread_once_t dtor_once = PTHREAD_ONCE_INIT;

/* Run in reverse registration order, including any registered meanwhile. */
static void run_dtors(void *head)
{
    while (head) {
        struct dtor_node *node = head;
        pthread_setspecific(dtor_key, NULL);
        node->dtor(node->obj);
        head = pthread_getspecific(dtor_key);
        if (head) {
            struct dtor_node *last = head;
            while (last->next) {
                last = last->next;
            }
            last->next = node->next;
        } else {
            head = node->next;
        }
        free(node);
    }
}

static void dtor_init(void)
{
    pthread_key_create(&dtor_key, run_dtors);
}

int __cxa_thread_atexit_impl(void (*dtor)(void *), void *obj, void *dso)
{
    (void)dso;
    pthread_once(&dtor_once, dtor_init);

    struct dtor_node *node = malloc(sizeof(*node));
    if (!node) {
        return -1;
    }
    node->dtor = dtor;
    node->obj = obj;
    node->next = pthread_getspecific(dtor_key);
    pthread_setspecific(dtor_key, node);
    return 0;
}
