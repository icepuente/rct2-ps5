/*
 * libc functions that the payload SDK's libc++ (locale, filesystem) and
 * OpenRCT2 use but the console's libc does not export to a native title.
 *
 * The app only runs in the "C" locale, so every *_l variant forwards to its
 * plain counterpart. The same approach is used by the Ship of Harkinian and
 * OpenMW PS5 ports.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <pwd.h>
#include <runetype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>
#include <xlocale.h>

extern const _RuneLocale _DefaultRuneLocale;

/* ---- locale objects ---------------------------------------------------- */

static int c_locale;

locale_t newlocale(int mask, const char *name, locale_t base)
{
    (void)mask;
    (void)name;
    (void)base;
    return (locale_t)&c_locale;
}

int freelocale(locale_t loc)
{
    (void)loc;
    return 0;
}

struct lconv *localeconv_l(locale_t loc)
{
    (void)loc;
    return localeconv();
}

int ___mb_cur_max_l(locale_t loc)
{
    (void)loc;
    return 1;
}

char *nl_langinfo(nl_item item)
{
    return item == CODESET ? "US-ASCII" : "";
}

/* ---- ctype --------------------------------------------------------------- */

_RuneLocale *__runes_for_locale(locale_t loc, int *mb_sb_limit)
{
    (void)loc;
    if (mb_sb_limit) {
        *mb_sb_limit = _CACHED_RUNES;
    }
    return (_RuneLocale *)&_DefaultRuneLocale;
}

const _RuneLocale *__getCurrentRuneLocale(void)
{
    return &_DefaultRuneLocale;
}

unsigned long ___runetype(__ct_rune_t c)
{
    return (c >= 0 && c < _CACHED_RUNES) ? _DefaultRuneLocale.__runetype[c] : 0;
}

unsigned long ___runetype_l(__ct_rune_t c, locale_t loc)
{
    (void)loc;
    return (c >= 0 && c < _CACHED_RUNES) ? _DefaultRuneLocale.__runetype[c] : 0;
}

__ct_rune_t ___tolower_l(__ct_rune_t c, locale_t loc)
{
    (void)loc;
    return (c >= 0 && c < _CACHED_RUNES) ? _DefaultRuneLocale.__maplower[c] : c;
}

__ct_rune_t ___toupper_l(__ct_rune_t c, locale_t loc)
{
    (void)loc;
    return (c >= 0 && c < _CACHED_RUNES) ? _DefaultRuneLocale.__mapupper[c] : c;
}

int iswctype_l(wint_t wc, wctype_t desc, locale_t loc)
{
    (void)loc;
    return iswctype(wc, desc);
}

/* ---- strings and numbers ------------------------------------------------- */

int strcoll_l(const char *a, const char *b, locale_t loc)
{
    (void)loc;
    return strcoll(a, b);
}

size_t strxfrm_l(char *dst, const char *src, size_t n, locale_t loc)
{
    (void)loc;
    return strxfrm(dst, src, n);
}

int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t loc)
{
    (void)loc;
    return wcscoll(a, b);
}

size_t wcsxfrm_l(wchar_t *dst, const wchar_t *src, size_t n, locale_t loc)
{
    (void)loc;
    return wcsxfrm(dst, src, n);
}

double strtod_l(const char *s, char **end, locale_t loc)
{
    (void)loc;
    return strtod(s, end);
}

float strtof_l(const char *s, char **end, locale_t loc)
{
    (void)loc;
    return strtof(s, end);
}

long double strtold_l(const char *s, char **end, locale_t loc)
{
    (void)loc;
    return strtold(s, end);
}

long long strtoll_l(const char *s, char **end, int base, locale_t loc)
{
    (void)loc;
    return strtoll(s, end, base);
}

unsigned long long strtoull_l(const char *s, char **end, int base, locale_t loc)
{
    (void)loc;
    return strtoull(s, end, base);
}

size_t strftime_l(char *s, size_t max, const char *fmt, const struct tm *tm,
                  locale_t loc)
{
    (void)loc;
    return strftime(s, max, fmt, tm);
}

int snprintf_l(char *s, size_t n, locale_t loc, const char *fmt, ...)
{
    va_list ap;
    (void)loc;
    va_start(ap, fmt);
    int r = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}

int asprintf_l(char **out, locale_t loc, const char *fmt, ...)
{
    va_list ap;
    (void)loc;
    va_start(ap, fmt);
    int r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

int sscanf_l(const char *s, locale_t loc, const char *fmt, ...)
{
    va_list ap;
    (void)loc;
    va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- multibyte conversion ------------------------------------------------ */

wint_t btowc_l(int c, locale_t loc)
{
    (void)loc;
    return btowc(c);
}

int wctob_l(wint_t c, locale_t loc)
{
    (void)loc;
    return wctob(c);
}

size_t mbrlen_l(const char *s, size_t n, mbstate_t *ps, locale_t loc)
{
    (void)loc;
    return mbrlen(s, n, ps);
}

size_t mbrtowc_l(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps, locale_t loc)
{
    (void)loc;
    return mbrtowc(pwc, s, n, ps);
}

int mbtowc_l(wchar_t *pwc, const char *s, size_t n, locale_t loc)
{
    (void)loc;
    return mbtowc(pwc, s, n);
}

size_t wcrtomb_l(char *s, wchar_t wc, mbstate_t *ps, locale_t loc)
{
    (void)loc;
    return wcrtomb(s, wc, ps);
}

size_t mbsrtowcs_l(wchar_t *dst, const char **src, size_t len, mbstate_t *ps,
                   locale_t loc)
{
    (void)loc;
    return mbsrtowcs(dst, src, len, ps);
}

/* Convert at most nms bytes from *src. */
size_t mbsnrtowcs_l(wchar_t *dst, const char **src, size_t nms, size_t len,
                    mbstate_t *ps, locale_t loc)
{
    static mbstate_t internal;
    const char *s = *src;
    size_t count = 0;
    (void)loc;

    if (!ps) {
        ps = &internal;
    }
    while (nms > 0 && (!dst || count < len)) {
        wchar_t wc;
        size_t r = mbrtowc(&wc, s, nms, ps);
        if (r == (size_t)-1) {
            if (dst) {
                *src = s;
            }
            return (size_t)-1;
        }
        if (r == (size_t)-2) {
            s += nms;
            break;
        }
        if (dst) {
            dst[count] = wc;
        }
        if (r == 0) {
            if (dst) {
                *src = NULL;
            }
            return count;
        }
        s += r;
        nms -= r;
        count++;
    }
    if (dst) {
        *src = s;
    }
    return count;
}

/* Convert at most nwc wide characters from *src. */
size_t wcsnrtombs_l(char *dst, const wchar_t **src, size_t nwc, size_t len,
                    mbstate_t *ps, locale_t loc)
{
    static mbstate_t internal;
    const wchar_t *s = *src;
    size_t count = 0;
    char buf[MB_LEN_MAX];
    (void)loc;

    if (!ps) {
        ps = &internal;
    }
    while (nwc > 0) {
        size_t r = wcrtomb(buf, *s, ps);
        if (r == (size_t)-1) {
            if (dst) {
                *src = s;
            }
            return (size_t)-1;
        }
        if (dst) {
            if (count + r > len) {
                break;
            }
            memcpy(dst + count, buf, r);
        }
        if (*s == L'\0') {
            if (dst) {
                *src = NULL;
            }
            return count + r - 1;
        }
        count += r;
        s++;
        nwc--;
    }
    if (dst) {
        *src = s;
    }
    return count;
}

/* ---- time ---------------------------------------------------------------- */

static pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;

struct tm *localtime_r(const time_t *t, struct tm *out)
{
    pthread_mutex_lock(&time_lock);
    struct tm *tm = localtime(t);
    if (tm) {
        *out = *tm;
    }
    pthread_mutex_unlock(&time_lock);
    return tm ? out : NULL;
}

/* ---- filesystem ---------------------------------------------------------- */

int utimensat(int dirfd, const char *path, const struct timespec ts[2], int flags)
{
    (void)flags;
    if (dirfd != AT_FDCWD && path[0] != '/') {
        errno = ENOSYS;
        return -1;
    }
    if (!ts) {
        return utimes(path, NULL);
    }
    struct timeval tv[2];
    for (int i = 0; i < 2; i++) {
        tv[i].tv_sec = ts[i].tv_sec;
        tv[i].tv_usec = ts[i].tv_nsec / 1000;
    }
    return utimes(path, tv);
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *path, struct dirent ***out,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **))
{
    DIR *dir = opendir(path);
    if (!dir) {
        return -1;
    }

    struct dirent **list = NULL;
    size_t count = 0, cap = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (filter && !filter(ent)) {
            continue;
        }
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            struct dirent **grown = realloc(list, cap * sizeof(*list));
            if (!grown) {
                goto fail;
            }
            list = grown;
        }
        struct dirent *copy = malloc(sizeof(*copy));
        if (!copy) {
            goto fail;
        }
        memcpy(copy, ent, sizeof(*copy));
        list[count++] = copy;
    }
    closedir(dir);
    if (compar) {
        qsort(list, count, sizeof(*list),
              (int (*)(const void *, const void *))compar);
    }
    *out = list;
    return (int)count;

fail:
    while (count > 0) {
        free(list[--count]);
    }
    free(list);
    closedir(dir);
    errno = ENOMEM;
    return -1;
}

/* ---- users, message catalogs, assertions ---------------------------------- */

struct passwd *getpwuid(uid_t uid)
{
    (void)uid;
    errno = ENOENT;
    return NULL;
}

typedef void *nl_catd_t;

nl_catd_t catopen(const char *name, int flag)
{
    (void)name;
    (void)flag;
    errno = ENOENT;
    return (nl_catd_t)-1;
}

char *catgets(nl_catd_t catd, int set, int msg, const char *def)
{
    (void)catd;
    (void)set;
    (void)msg;
    return (char *)def;
}

int catclose(nl_catd_t catd)
{
    (void)catd;
    return 0;
}

void __assert(const char *func, const char *file, int line, const char *expr)
{
    printf("Assertion failed: (%s), function %s, file %s, line %d.\n",
           expr, func ? func : "?", file, line);
    abort();
}
