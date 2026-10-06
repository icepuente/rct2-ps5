/*
 * Shims for running payload-SDK libraries (SDL2 and friends) in a native PS5
 * title.
 *
 * A native title only gets the system modules the loader preloads. Imports
 * from any other module link fine but stay null, so the first call jumps to
 * address 0. The functions below replace such imports: system functions are
 * resolved lazily from their module, and libc functions that only
 * libScePosixForWebKit provides are implemented here.
 *
 * Check a new binary with scripts/check-imports.sh.
 */
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

int sceKernelLoadStartModule(const char *, size_t, const void *, unsigned int,
                             void *, int *);
int sceKernelDlsym(int, const char *, void **);
const char *sceKernelGetFsSandboxRandomWord(void);
unsigned long long sceKernelGetProcessTime(void);

/* Load a module from the title's view of /system/common/lib. */
static int load_module(const char *name)
{
    char path[128];

    snprintf(path, sizeof(path), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), name);
    int handle = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    if (handle < 0) {
        snprintf(path, sizeof(path), "/system/common/lib/%s", name);
        handle = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    }
    return handle;
}

static void *resolve(int *handle, const char *module, const char *symbol)
{
    void *addr = NULL;

    if (*handle == 0) {
        *handle = load_module(module);
    }
    if (*handle < 0 || sceKernelDlsym(*handle, symbol, &addr) != 0) {
        return NULL;
    }
    return addr;
}

/*
 * LAZY_IMPORT(module_handle, module, ret, fail, name, params, args) defines
 * `name` to forward to the real function, returning `fail` when the module
 * or symbol is unavailable.
 */
#define LAZY_IMPORT(handle, module, ret, fail, name, params, args)      \
    ret name params                                                     \
    {                                                                   \
        static ret (*real) params;                                      \
        if (!real) {                                                    \
            real = (ret (*) params)resolve(&handle, module, #name);    \
        }                                                               \
        return real ? real args : (fail);                               \
    }

#define MODULE_UNAVAILABLE ((int)0x80020002) /* SCE_KERNEL_ERROR_ENOENT */

static int keyboard_module;
static int ime_dialog_module;

LAZY_IMPORT(keyboard_module, "libSceKeyboard.sprx", int, MODULE_UNAVAILABLE,
            sceKeyboardInit, (void), ())
LAZY_IMPORT(keyboard_module, "libSceKeyboard.sprx", int, MODULE_UNAVAILABLE,
            sceKeyboardOpen, (int a, int b, int c, void *d), (a, b, c, d))
LAZY_IMPORT(keyboard_module, "libSceKeyboard.sprx", int, MODULE_UNAVAILABLE,
            sceKeyboardReadState, (int a, void *b), (a, b))
LAZY_IMPORT(keyboard_module, "libSceKeyboard.sprx", int, MODULE_UNAVAILABLE,
            sceKeyboardClose, (int a), (a))

LAZY_IMPORT(ime_dialog_module, "libSceImeDialog.sprx", int, MODULE_UNAVAILABLE,
            sceImeDialogInit, (const void *a, void *b), (a, b))
LAZY_IMPORT(ime_dialog_module, "libSceImeDialog.sprx", int, 0,
            sceImeDialogGetStatus, (void), ()) /* 0 = SCE_IME_DIALOG_STATUS_NONE */
LAZY_IMPORT(ime_dialog_module, "libSceImeDialog.sprx", int, MODULE_UNAVAILABLE,
            sceImeDialogGetResult, (void *a), (a))
LAZY_IMPORT(ime_dialog_module, "libSceImeDialog.sprx", int, MODULE_UNAVAILABLE,
            sceImeDialogTerm, (void), ())

/*
 * A native title has no stdout or stderr, so writes to them go to the kernel
 * log (read it with klogsrv on port 3232), one line at a time. printf and
 * friends are defined here; vfprintf, fputs, fputc, fwrite and fflush are
 * wrapped by scripts/package-native.sh so other streams still work.
 */
int sceKernelDebugOutText(int, const char *);
int __real_vfprintf(FILE *, const char *, va_list);
int __real_fputs(const char *, FILE *);
int __real_fputc(int, FILE *);
size_t __real_fwrite(const void *, size_t, size_t, FILE *);
int __real_fflush(FILE *);

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static char log_line[1024];
static size_t log_len;

static void log_flush_locked(void)
{
    if (log_len > 0) {
        log_line[log_len] = '\0';
        sceKernelDebugOutText(0, log_line);
        log_len = 0;
    }
}

static void log_write(const char *s, size_t n)
{
    pthread_mutex_lock(&log_lock);
    for (size_t i = 0; i < n; i++) {
        log_line[log_len++] = s[i];
        if (s[i] == '\n' || log_len == sizeof(log_line) - 1) {
            log_flush_locked();
        }
    }
    pthread_mutex_unlock(&log_lock);
}

static int is_console(FILE *stream)
{
    return stream == stdout || stream == stderr;
}

int vprintf(const char *fmt, va_list ap)
{
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);

    log_write(buf, n < 0 ? 0 : (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1);
    return n;
}

int printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int puts(const char *s)
{
    log_write(s, strlen(s));
    log_write("\n", 1);
    return 0;
}

#undef putchar
int putchar(int c)
{
    char ch = (char)c;
    log_write(&ch, 1);
    return c;
}

int __wrap_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    return is_console(stream) ? vprintf(fmt, ap) : __real_vfprintf(stream, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int n = __wrap_vfprintf(stream, fmt, ap);
    va_end(ap);
    return n;
}

int __wrap_fputs(const char *s, FILE *stream)
{
    if (!is_console(stream)) {
        return __real_fputs(s, stream);
    }
    log_write(s, strlen(s));
    return 0;
}

int __wrap_fputc(int c, FILE *stream)
{
    if (!is_console(stream)) {
        return __real_fputc(c, stream);
    }
    char ch = (char)c;
    log_write(&ch, 1);
    return c;
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t n, FILE *stream)
{
    if (!is_console(stream)) {
        return __real_fwrite(ptr, size, n, stream);
    }
    log_write(ptr, size * n);
    return n;
}

int __wrap_fflush(FILE *stream)
{
    if (!stream || is_console(stream)) {
        pthread_mutex_lock(&log_lock);
        log_flush_locked();
        pthread_mutex_unlock(&log_lock);
        if (stream) {
            return 0;
        }
    }
    return __real_fflush(stream);
}

/*
 * A native title may not exit the process itself (the exit syscall raises
 * SIGSYS), so exit() (wrapped by scripts/package-native.sh) asks the system
 * to close the app instead.
 */
int sceSystemServiceLoadExec(const char *, const char **);
int sceKernelUsleep(unsigned int);

void __wrap_exit(int status)
{
    printf("exit(%d): closing the app\n", status);
    sceSystemServiceLoadExec("exit", NULL);
    for (;;) {
        sceKernelUsleep(1000000);
    }
}

/* zstd's optional tracing hooks; 0 means "not traced". */
unsigned long long ZSTD_trace_compress_begin(const void *cctx)
{
    (void)cctx;
    return 0;
}

void ZSTD_trace_compress_end(unsigned long long ctx, const void *trace)
{
    (void)ctx;
    (void)trace;
}

unsigned long long ZSTD_trace_decompress_begin(const void *dctx)
{
    (void)dctx;
    return 0;
}

void ZSTD_trace_decompress_end(unsigned long long ctx, const void *trace)
{
    (void)ctx;
    (void)trace;
}

/*
 * The sandbox refuses KERN_PROC_PATHNAME, which apps use to find their own
 * executable (OpenRCT2 treats the failure as fatal). Answer it with the
 * title's eboot; pass every other query through.
 */
int __real_sysctl(const int *, unsigned int, void *, size_t *, const void *, size_t);

int __wrap_sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
                  const void *newp, size_t newlen)
{
    static const char exe_path[] = "/app0/eboot.bin";

    if (namelen == 4 && name[0] == CTL_KERN && name[1] == KERN_PROC &&
        name[2] == KERN_PROC_PATHNAME && !newp) {
        if (oldp) {
            if (!oldlenp || *oldlenp < sizeof(exe_path)) {
                errno = ENOMEM;
                return -1;
            }
            memcpy(oldp, exe_path, sizeof(exe_path));
        }
        if (oldlenp) {
            *oldlenp = sizeof(exe_path);
        }
        return 0;
    }
    return __real_sysctl(name, namelen, oldp, oldlenp, newp, newlen);
}

/*
 * A native title cannot start other processes. Fail cleanly instead of
 * calling a null import (OpenRCT2 probes for zenity this way).
 */
pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

pid_t vfork(void)
{
    errno = ENOSYS;
    return -1;
}

int system(const char *command)
{
    /* system(NULL) asks whether a shell exists. */
    if (!command) {
        return 0;
    }
    errno = ENOSYS;
    return -1;
}

FILE *popen(const char *command, const char *mode)
{
    (void)command;
    (void)mode;
    errno = ENOSYS;
    return NULL;
}

/*
 * Functions below are only exported by libScePosixForWebKit, which a native
 * title never loads.
 */

int isatty(int fd)
{
    (void)fd;
    errno = ENOTTY;
    return 0;
}

/* Glob matching with *, ? and [...]; flags are not supported. */
int fnmatch(const char *pattern, const char *string, int flags)
{
    (void)flags;
    while (*pattern) {
        switch (*pattern) {
        case '*':
            while (*pattern == '*') {
                pattern++;
            }
            if (!*pattern) {
                return 0;
            }
            for (; *string; string++) {
                if (fnmatch(pattern, string, flags) == 0) {
                    return 0;
                }
            }
            return 1;
        case '?':
            if (!*string) {
                return 1;
            }
            break;
        case '[': {
            const char *p = pattern + 1;
            int negate = (*p == '!' || *p == '^');
            int found = 0;
            if (negate) {
                p++;
            }
            for (; *p && (*p != ']' || p == pattern + 1 + negate); p++) {
                if (p[1] == '-' && p[2] && p[2] != ']') {
                    found |= *string >= p[0] && *string <= p[2];
                    p += 2;
                } else {
                    found |= *string == *p;
                }
            }
            if (!*p || !*string || found == negate) {
                return 1;
            }
            pattern = p;
            break;
        }
        default:
            if (*pattern != *string) {
                return 1;
            }
        }
        pattern++;
        string++;
    }
    return *string ? 1 : 0;
}

static int random_module;
LAZY_IMPORT(random_module, "libSceRandom.sprx", int, -1,
            sceRandomGetRandomNumber, (void *buf, size_t size), (buf, size))

void arc4random_buf(void *buf, size_t size)
{
    unsigned char *out = buf;

    /* sceRandomGetRandomNumber returns at most 64 bytes per call. */
    while (size > 0) {
        size_t chunk = size < 64 ? size : 64;
        if (sceRandomGetRandomNumber(out, chunk) != 0) {
            break;
        }
        out += chunk;
        size -= chunk;
    }

    /* Fallback: splitmix64 seeded from the clock. Not for cryptography. */
    static unsigned long long state;
    if (size > 0 && state == 0) {
        state = sceKernelGetProcessTime() ^ 0x9e3779b97f4a7c15ULL;
    }
    while (size > 0) {
        unsigned long long z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        size_t chunk = size < sizeof(z) ? size : sizeof(z);
        memcpy(out, &z, chunk);
        out += chunk;
        size -= chunk;
    }
}

unsigned int arc4random(void)
{
    unsigned int value;
    arc4random_buf(&value, sizeof(value));
    return value;
}

/* Only libScePosixForWebKit exports strcasestr. */
char *strcasestr(const char *haystack, const char *needle)
{
    size_t len = strlen(needle);

    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, len) == 0) {
            return (char *)haystack;
        }
    }
    return len == 0 ? (char *)haystack : NULL;
}
